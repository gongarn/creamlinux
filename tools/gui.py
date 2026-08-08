#!/usr/bin/env python3
"""
gui.py - web UI for creamlinux tools.

Serves a local web app on http://127.0.0.1:8765 (no domain, no internet
required - everything runs on your machine). Uses the shared creamlib core.

Usage:
  python3 tools/gui.py                # start and open the browser
  python3 tools/gui.py --port 9000    # custom port
  python3 tools/gui.py --no-browser   # don't auto-open the browser
"""

import argparse
import collections
import json
import os
import sys
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import creamlib  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
STATIC_DIR = os.path.join(HERE, "gui_static")
DEFAULT_PORT = 8765

LOG_TAIL = collections.deque(maxlen=500)
LOG_LOCK = threading.Lock()


def log_line(msg):
    with LOG_LOCK:
        LOG_TAIL.append(str(msg))


def log_tail(n=200):
    with LOG_LOCK:
        return list(LOG_TAIL)[-n:]


# ------------------------------------------------------- background jobs ---

class JobManager:
    """Runs one install/uninstall/dlc-update job at a time in a thread."""

    def __init__(self):
        self._lock = threading.Lock()
        self._thread = None
        self._running = False

    def start(self, fn, *args):
        with self._lock:
            if self._running:
                raise RuntimeError("another operation is already running")
            self._running = True
            self._thread = threading.Thread(target=self._run,
                                            args=(fn, args), daemon=True)
            self._thread.start()

    def _run(self, fn, args):
        try:
            fn(*args)
        except Exception as exc:  # noqa: BLE001
            log_line(f"error: {exc}")
        finally:
            with self._lock:
                self._running = False

    def busy(self):
        with self._lock:
            return self._running


JOBS = JobManager()


# --------------------------------------------------------------- helpers ---

def json_response(handler, payload, status=200):
    body = json.dumps(payload).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(body)))
    handler.send_header("Cache-Control", "no-store")
    handler.end_headers()
    handler.wfile.write(body)


def read_json_body(handler):
    length = int(handler.headers.get("Content-Length") or 0)
    if length <= 0:
        return {}
    try:
        return json.loads(handler.rfile.read(length).decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        return {}


def require_games(handler, body):
    """Resolve appid(s) -> game dicts via scan (never trust raw paths).
    Accepts either a single 'appid' or an 'appids' list."""
    raw = body.get("appids")
    if raw is None and body.get("appid") is not None:
        raw = [body.get("appid")]
    if not isinstance(raw, list) or not raw:
        json_response(handler, {"error": "appids list is required"}, status=400)
        return None
    games = {g["appid"]: g for g in creamlib.scan_games()}
    result = []
    for appid in raw:
        game = games.get(str(appid))
        if game is None:
            json_response(handler, {"error": f"app {appid} not found in Steam "
                                              "libraries"}, status=404)
            return None
        result.append(game)
    return result


# ------------------------------------------------------------------ API ----

def api_games(handler):
    try:
        games = creamlib.scan_games()
    except Exception as exc:  # noqa: BLE001
        json_response(handler, {"error": str(exc)}, status=500)
        return
    payload = [{
        "appid": g["appid"], "name": g["name"], "game_dir": g["game_dir"],
        "game_type": g["game_type"], "installed": g["installed"],
        "dlc_status": g.get("dlc_status", "unknown"),
    } for g in sorted(games, key=lambda x: x["name"].lower())]
    json_response(handler, {"games": payload, "busy": JOBS.busy()})


def api_install(handler, body):
    games = require_games(handler, body)
    if games is None:
        return
    mode = body.get("smokeapi_mode", "hook")
    unlockall = body.get("unlockall", False)
    dry_run = bool(body.get("dry_run"))
    if mode not in ("hook", "koaloader", "proxy"):
        json_response(handler, {"error": f"unknown smokeapi mode {mode}"},
                      status=400)
        return

    def job():
        for game in games:
            log_line(f"=== Installing for {game['name']} "
                     f"({game['game_type']}, {game['game_dir']}) ===")
            if game["game_type"] == "native":
                dist = creamlib.find_creamlinux_dist()
                if dist is None:
                    log_line("error: no local creamlinux build found; "
                             "run 'sh ./build.sh' first")
                    continue
                creamlib.install_native(game["game_dir"], dist, dry_run,
                                        verbose=False, log=log_line)
            elif game["game_type"] == "proton":
                creamlib.install_proton(game["game_dir"], dry_run=dry_run,
                                        mode=mode, log=log_line)
            else:
                log_line("error: no Steam API files found in this game")
                continue
            if unlockall and not dry_run:
                ini = creamlib.read_game_ini(game["game_dir"]) or {}
                config = ini.get("config", {})
                config["unlockall"] = "true"
                creamlib.write_game_ini(
                    game["game_dir"], config, ini.get("methods", {}),
                    ini.get("dlc", []), log=log_line)
                log_line("unlockall = true written to cream_api.ini")
        log_line("=== Done ===")

    try:
        JOBS.start(job)
    except RuntimeError as exc:
        json_response(handler, {"error": str(exc)}, status=409)
        return
    json_response(handler, {"started": True})


def api_uninstall(handler, body):
    games = require_games(handler, body)
    if games is None:
        return
    remove_ini = bool(body.get("remove_ini", True))

    def job():
        for game in games:
            log_line(f"=== Removing unlocker from {game['name']} ===")
            if game["game_type"] == "native":
                creamlib.uninstall_native(game["game_dir"],
                                          remove_ini=remove_ini, log=log_line)
            elif game["game_type"] == "proton":
                creamlib.uninstall_proton(game["game_dir"],
                                          remove_ini=remove_ini, log=log_line)
            else:
                log_line("Nothing to uninstall.")
        log_line("=== Done ===")

    try:
        JOBS.start(job)
    except RuntimeError as exc:
        json_response(handler, {"error": str(exc)}, status=409)
        return
    json_response(handler, {"started": True})

def api_dlc_update(handler, body):
    game = require_games(handler, body)
    if game is None:
        return
    game = game[0]
    dry_run = bool(body.get("dry_run"))

    def job():
        log_line(f"=== Updating DLC list for {game['name']} ===")
        creamlib.run_dlc_update(game["appid"], dry_run=dry_run, log=log_line)
        log_line("=== Done ===")

    try:
        JOBS.start(job)
    except RuntimeError as exc:
        json_response(handler, {"error": str(exc)}, status=409)
        return
    json_response(handler, {"started": True})


def api_game_config(handler, body):
    games = require_games(handler, body)
    if games is None:
        return
    game = games[0]
    if game is None:
        return
    if body.get("read"):
        ini = creamlib.read_game_ini(game["game_dir"])
        if ini is None:
            json_response(handler, {"error": "no cream_api.ini in this game"},
                          status=404)
            return
        json_response(handler, ini)
        return
    # write config/methods/dlc
    ini = creamlib.read_game_ini(game["game_dir"]) or {"config": {},
                                                       "methods": {}, "dlc": []}
    if "config" in body:
        ini["config"].update({k: str(v) for k, v in body["config"].items()})
    if "methods" in body:
        ini["methods"].update({k: str(v) for k, v in body["methods"].items()})
    if "dlc" in body:
        ini["dlc"] = [[str(k), str(v)] for k, v in body["dlc"]]
    try:
        creamlib.write_game_ini(game["game_dir"], ini["config"],
                                ini["methods"], ini["dlc"], log=log_line)
    except OSError as exc:
        json_response(handler, {"error": str(exc)}, status=500)
        return
    json_response(handler, {"ok": True})


def api_log(handler):
    json_response(handler, {"lines": log_tail(), "busy": JOBS.busy()})


def api_open_folder(handler, body):
    games = require_games(handler, body)
    if games is None:
        return
    game = games[0]
    if game is None:
        return
    path = game["game_dir"]
    if not os.path.isdir(path):
        json_response(handler, {"error": f"{path} is not a directory"},
                      status=404)
        return
    try:
        import subprocess
        subprocess.Popen(["xdg-open", path])
    except OSError as exc:
        json_response(handler, {"error": str(exc)}, status=500)
        return
    json_response(handler, {"ok": True})


# ---------------------------------------------------------------- server ---

ROUTES = {
    "GET": {
        "/api/games": api_games,
        "/api/log": api_log,
    },
    "POST": {
        "/api/install": api_install,
        "/api/uninstall": api_uninstall,
        "/api/dlc-update": api_dlc_update,
        "/api/game-config": api_game_config,
        "/api/open-folder": api_open_folder,
    },
}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):  # silence default logging
        pass

    def _serve_static(self, path):
        rel = path.lstrip("/")
        if rel == "":
            rel = "index.html"
        # prevent path traversal
        full = os.path.realpath(os.path.join(STATIC_DIR, rel))
        if not full.startswith(os.path.realpath(STATIC_DIR) + os.sep) and \
                full != os.path.realpath(STATIC_DIR):
            self.send_error(403)
            return
        if not os.path.isfile(full):
            self.send_error(404)
            return
        ctype = "text/html" if full.endswith(".html") else \
                "application/javascript" if full.endswith(".js") else \
                "text/css" if full.endswith(".css") else \
                "application/octet-stream"
        with open(full, "rb") as fh:
            body = fh.read()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/" or self.path.startswith("/static/"):
            path = self.path.removeprefix("/static/") if \
                self.path.startswith("/static/") else "/"
            self._serve_static(path)
            return
        route = ROUTES["GET"].get(self.path.split("?")[0])
        if route is None:
            self.send_error(404)
            return
        try:
            route(self)
        except Exception as exc:  # noqa: BLE001
            json_response(self, {"error": str(exc)}, status=500)

    def do_POST(self):
        route = ROUTES["POST"].get(self.path)
        if route is None:
            self.send_error(404)
            return
        body = read_json_body(self)
        try:
            route(self, body)
        except Exception as exc:  # noqa: BLE001
            json_response(self, {"error": str(exc)}, status=500)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=DEFAULT_PORT)
    ap.add_argument("--no-browser", action="store_true")
    args = ap.parse_args()

    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    url = f"http://127.0.0.1:{args.port}"
    print(f"creamlinux web UI: {url}  (Ctrl+C to stop)")
    if not args.no_browser:
        threading.Timer(0.5, lambda: webbrowser.open(url)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")


if __name__ == "__main__":
    main()
