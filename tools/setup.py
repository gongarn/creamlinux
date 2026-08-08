#!/usr/bin/env python3
"""
setup.py - install DLC unlockers into a game folder.

One tool for both cases:
  * native Linux games      -> creamlinux  (LD_PRELOAD hooks)
  * Windows games in Proton -> SmokeAPI    (DLL proxy/hook, by acidicoala)

The cream_api.ini DLC list is shared between both unlockers.

Usage:
  python3 tools/setup.py --dir /path/to/game                  # auto-detect mode
  python3 tools/setup.py --dir /path/to/game --mode native
  python3 tools/setup.py --dir /path/to/game --mode proton
  python3 tools/setup.py --dir /path/to/game --update-dlc 394360
  python3 tools/setup.py --dir /path/to/game --dry-run        # preview only
  python3 tools/setup.py --scan                               # list installed Steam games
  python3 tools/setup.py --install 394360                     # install unlocker for an appid

Notes:
  - For native mode, creamlinux files are taken from the local checkout
    (build output or the repo package/ folder) or, if missing, downloaded
    from the latest GitHub release of this fork.
  - SmokeAPI limitations (anti-cheat, Denuvo SecureDLC, 3rd party DRM)
    apply; see https://github.com/acidicoala/SmokeAPI for details.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

USER_AGENT = "creamlinux-setup/1.0"

SMOKEAPI_REPO = "acidicoala/SmokeAPI"
CREAMLINUX_REPO = "gongarn/creamlinux"

CREAM_FILES = ["lib64Creamlinux.so", "lib32Creamlinux.so", "cream.sh",
               "cream_api.ini"]


# ------------------------------------------------------------------ http ---

def http_get(url, timeout=60):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read()


def latest_release_asset(repo):
    """Return (asset_name, browser_download_url) of the latest release."""
    data = json.loads(http_get(f"https://api.github.com/repos/{repo}/releases/latest"))
    assets = data.get("assets") or []
    if not assets:
        raise RuntimeError(f"repo {repo} has no release assets")
    asset = assets[0]
    return asset["name"], asset["browser_download_url"]


# -------------------------------------------------------------- detection --

def detect_mode(game_dir):
    if os.path.exists(os.path.join(game_dir, "libsteam_api.so")):
        return "native"
    if os.path.exists(os.path.join(game_dir, "steam_api64.dll")) or \
       os.path.exists(os.path.join(game_dir, "steam_api.dll")):
        return "proton"
    return None


def detect_bitness(game_dir):
    """Return (bitness, dll_dir) or (None, None). dll_dir is where the
    Steam API dll lives - the proxy dll must be placed next to it (i.e.
    next to the game executable)."""
    found_type, api_path = find_steam_api_files(game_dir)
    if found_type != "proton" or api_path is None:
        return None, None
    dll_dir = os.path.dirname(api_path)
    if os.path.basename(api_path) == "steam_api64.dll":
        return 64, dll_dir
    return 32, dll_dir


# --------------------------------------------------------------- native ----

def find_creamlinux_dist():
    """Locate a local creamlinux distribution (built checkout or release zip
    next to the script). Returns a temp dir with the files or None."""
    here = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(here)
    candidates = [
        os.path.join(repo_root, "output"),              # after build.sh
        os.path.join(repo_root, "build", "64", "lib"),  # cmake build dir
        here,                                            # files next to script
    ]
    for cand in candidates:
        if all(os.path.exists(os.path.join(cand, f)) for f in CREAM_FILES):
            return cand
    return None


def install_native(game_dir, dist_dir, dry_run, verbose):
    plan = []
    for f in CREAM_FILES:
        plan.append(("copy", os.path.join(dist_dir, f),
                     os.path.join(game_dir, f)))
    if dry_run:
        print("Native mode plan:")
        for _, src, dst in plan:
            print(f"  cp {src} -> {dst}")
        return 0
    for _, src, dst in plan:
        if verbose:
            print(f"copying {src} -> {dst}")
        shutil.copy2(src, dst)
    print("Installed creamlinux. Set the game's Steam launch options to:")
    print("  sh ./cream.sh %command%")
    print("(with MangoHud:  sh ./cream.sh mangohud %command%)")
    return 0


# --------------------------------------------------------------- proton ----

def download_smokeapi(cache_dir):
    zip_path = os.path.join(cache_dir, "smokeapi-latest.zip")
    if os.path.exists(zip_path):
        print(f"Using cached SmokeAPI: {zip_path}")
        return zip_path
    os.makedirs(cache_dir, exist_ok=True)
    print("Downloading SmokeAPI...")
    name, url = latest_release_asset(SMOKEAPI_REPO)
    data = http_get(url)
    with open(zip_path, "wb") as fh:
        fh.write(data)
    print(f"  saved {name} ({len(data)} bytes)")
    return zip_path


def install_proton(game_dir, cache_dir, dry_run, verbose):
    bitness, dll_dir = detect_bitness(game_dir)
    if bitness is None:
        print("error: no steam_api.dll / steam_api64.dll found in the game "
              "directory - SmokeAPI cannot work here", file=sys.stderr)
        return 1
    zip_path = download_smokeapi(cache_dir)
    with zipfile.ZipFile(zip_path) as zf:
        dll_name = f"smoke_api{bitness}.dll"
        if dll_name not in zf.namelist():
            print(f"error: {dll_name} missing in SmokeAPI archive",
                  file=sys.stderr)
            return 1
        # Hook mode: rename the dll so the game loads it as a system dll.
        # 64-bit games -> version.dll, 32-bit -> winhttp.dll (avoids clashes
        # when both are installed next to each other).
        target = "version.dll" if bitness == 64 else "winhttp.dll"
        if dry_run:
            print("Proton mode plan (hook mode, self-hook):")
            print(f"  extract {dll_name} -> {dll_dir}/{target}")
            print("  copy cream_api.ini -> " + game_dir)
            return 0
        tmp = tempfile.mktemp(suffix=".dll")
        with open(tmp, "wb") as out:
            out.write(zf.read(dll_name))
        shutil.copy2(tmp, os.path.join(dll_dir, target))
        os.remove(tmp)
    # share the DLC list
    dist = find_creamlinux_dist()
    if dist:
        shutil.copy2(os.path.join(dist, "cream_api.ini"),
                     os.path.join(game_dir, "cream_api.ini"))
        print(f"copied cream_api.ini (from {dist})")
    else:
        print("warning: no local cream_api.ini found; copy one manually "
              "into the game folder", file=sys.stderr)
    print(f"Installed SmokeAPI ({bitness}-bit, hook mode) as {target}.")
    print("No launch options needed - the DLL is loaded automatically.")
    print("If the game does not load it, try proxy mode (see "
          "https://github.com/acidicoala/SmokeAPI) or rename to winmm.dll.")
    return 0


# ----------------------------------------------------------------- scan ----
# Minimal VDF parser: "key" "value" pairs and "key" { ... } blocks.

def parse_vdf(text):
    tokens = []
    i = 0
    while i < len(text):
        ch = text[i]
        if ch in ' \t\r\n':
            i += 1
        elif ch == '"':
            j = text.index('"', i + 1)
            tokens.append(text[i + 1:j])
            i = j + 1
        elif ch == '{':
            tokens.append("{")
            i += 1
        elif ch == '}':
            tokens.append("}")
            i += 1
        else:
            i += 1
    return tokens


def vdf_to_dict(tokens):
    """Tokens -> nested dict; repeated keys become lists."""
    def parse(pos):
        result = {}
        while pos < len(tokens):
            key = tokens[pos]
            if key == "}":
                return result, pos + 1
            if pos + 1 >= len(tokens):
                break
            nxt = tokens[pos + 1]
            if nxt == "{":
                value, pos = parse(pos + 2)
            else:
                value = nxt
                pos += 2
            if key in result:
                if not isinstance(result[key], list):
                    result[key] = [result[key]]
                result[key].append(value)
            else:
                result[key] = value
        return result, pos

    result, _ = parse(0)
    return result


def find_steam_library_roots():
    """Locate steamapps/libraryfolders.vdf across known install locations."""
    candidates = []
    home = os.path.expanduser("~")
    for p in (os.path.join(home, ".steam", "steam"),
              os.path.join(home, ".local", "share", "Steam"),
              os.path.join(home, ".steam", "root")):
        if os.path.isdir(p) and p not in candidates:
            candidates.append(p)
    vdfs = [os.path.join(p, "steamapps", "libraryfolders.vdf")
            for p in candidates]
    vdfs = [v for v in vdfs if os.path.exists(v)]
    return vdfs


def steam_libraries():
    """Yield (library_path, steamapps_dir) for every Steam library folder."""
    seen = set()
    for vdf in find_steam_library_roots():
        try:
            with open(vdf, "r", encoding="utf-8", errors="replace") as fh:
                data = vdf_to_dict(parse_vdf(fh.read()))
        except Exception:  # noqa: BLE001
            continue
        folders = data.get("libraryfolders", {})
        if isinstance(folders, dict):
            for entry in folders.values():
                if isinstance(entry, dict) and entry.get("path"):
                    steamapps = os.path.join(entry["path"], "steamapps")
                    if os.path.isdir(steamapps) and steamapps not in seen:
                        seen.add(steamapps)
                        yield entry["path"], steamapps
        # legacy flat format: "0" "path"
        if isinstance(folders, dict):
            for value in folders.values():
                if isinstance(value, str) and os.path.isdir(value):
                    steamapps = os.path.join(value, "steamapps")
                    if os.path.isdir(steamapps) and steamapps not in seen:
                        seen.add(steamapps)
                        yield value, steamapps


TOOL_APPS = {  # Steam infrastructure, not games
    "1070560": "Steam Linux Runtime 1.0 (scout)",
    "1391110": "Steam Linux Runtime 3.0 (sniper)",
    "1628350": "Steam Linux Runtime 4.0 (soldier)",
    "1493710": "Steam Linux Runtime 5.0 (soldier)",
    "4183110": "Steam Linux Runtime 4.0",
    "228980": "Steamworks Common Redistributables",
    "2805730": "Proton Experimental",
}


def find_steam_api_files(game_dir, max_depth=3):
    """Find libsteam_api.so / steam_api*.dll inside a game folder.
    Returns ('native', path) or ('proton', path) or (None, None)."""
    root_depth = game_dir.rstrip(os.sep).count(os.sep)
    for dirpath, dirnames, filenames in os.walk(game_dir):
        depth = dirpath.rstrip(os.sep).count(os.sep) - root_depth
        if depth >= max_depth:
            dirnames[:] = []
            continue
        for fname in filenames:
            if fname == "libsteam_api.so":
                return "native", os.path.join(dirpath, fname)
            if fname in ("steam_api.dll", "steam_api64.dll"):
                return "proton", os.path.join(dirpath, fname)
        if depth >= max_depth - 1:
            dirnames[:] = []
    return None, None


def scan_games():
    """Return list of dicts: appid, name, installdir, lib_path, game_dir,
    game_type ('native'/'proton'/None), installed ('creamlinux'/'smokeapi'/None)."""
    games = []
    for lib_path, steamapps in steam_libraries():
        for acf in sorted(os.listdir(steamapps)):
            if not (acf.startswith("appmanifest_") and acf.endswith(".acf")):
                continue
            appid = acf[len("appmanifest_"):-len(".acf")]
            if appid in TOOL_APPS:
                continue
            try:
                with open(os.path.join(steamapps, acf), "r",
                          encoding="utf-8", errors="replace") as fh:
                    data = vdf_to_dict(parse_vdf(fh.read()))
                state = data.get("AppState", {})
            except Exception:  # noqa: BLE001
                continue
            name = state.get("name", "?")
            installdir = state.get("installdir", "")
            if not installdir:
                continue
            game_dir = os.path.join(lib_path, "steamapps", "common", installdir)
            if not os.path.isdir(game_dir):
                continue
            game_type = None
            installed = None
            found_type, api_path = find_steam_api_files(game_dir)
            if found_type == "native":
                game_type = "native"
                if (os.path.exists(os.path.join(game_dir, "cream.sh")) and
                        os.path.exists(os.path.join(game_dir, "cream_api.ini"))):
                    installed = "creamlinux"
            elif found_type == "proton":
                game_type = "proton"
                if (os.path.exists(os.path.join(game_dir, "version.dll")) or
                        os.path.exists(os.path.join(game_dir, "winhttp.dll"))):
                    installed = "smokeapi"
            games.append({"appid": appid, "name": name, "game_dir": game_dir,
                          "game_type": game_type, "installed": installed})
    return games


def cmd_scan(args):
    games = scan_games()
    if not games:
        print("No Steam games found (checked libraryfolders.vdf in "
              "~/.steam, ~/.local/share/Steam).")
        return 1
    print(f"{'APPID':<10}{'TYPE':<8}{'UNLOCKER':<12}GAME")
    print("-" * 70)
    for g in sorted(games, key=lambda x: x["name"].lower()):
        print(f"{g['appid']:<10}{str(g['game_type'] or '-'):<8}"
              f"{str(g['installed'] or '-'):<12}{g['name']}")
    print()
    print("Install with:  python3 tools/setup.py --install <APPID>")
    return 0


def cmd_install(args):
    games = scan_games()
    match = [g for g in games if g["appid"] == str(args.install)]
    if not match:
        print(f"error: app {args.install} not found in Steam libraries",
              file=sys.stderr)
        return 1
    game = match[0]
    print(f"Installing unlocker for {game['name']} "
          f"({game['game_type']}, {game['game_dir']})")
    if game["game_type"] == "native":
        dist = find_creamlinux_dist()
        if dist is None:
            print("error: no local creamlinux build found; run 'sh ./build.sh' "
                  "first or pass --dir with a prebuilt dist", file=sys.stderr)
            return 1
        return install_native(game["game_dir"], dist, args.dry_run, args.verbose)
    if game["game_type"] == "proton":
        cache = os.path.join(os.path.expanduser("~"), ".cache", "creamlinux")
        return install_proton(game["game_dir"], cache, args.dry_run, args.verbose)
    print(f"error: {game['name']} has no Steam API files "
          "(libsteam_api.so / steam_api*.dll)", file=sys.stderr)
    return 1


# ----------------------------------------------------------------- main ----

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", help="game installation directory")
    ap.add_argument("--mode", choices=["auto", "native", "proton"],
                    default="auto")
    ap.add_argument("--scan", action="store_true",
                    help="list installed Steam games and their unlocker status")
    ap.add_argument("--install", type=int, metavar="APPID",
                    help="install the unlocker for an installed Steam game")
    ap.add_argument("--update-dlc", type=int, metavar="APPID",
                    help="run tools/update-dlc.py for this app first")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the plan without changing anything")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if args.scan:
        return cmd_scan(args)
    if args.install is not None:
        return cmd_install(args)
    if not args.dir:
        ap.error("one of --dir, --scan or --install is required")

    game_dir = os.path.abspath(args.dir)
    if not os.path.isdir(game_dir):
        print(f"error: {game_dir} is not a directory", file=sys.stderr)
        return 1

    # optional: refresh the DLC list first
    if args.update_dlc:
        script = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              "update-dlc.py")
        if not os.path.exists(script):
            print(f"error: {script} not found", file=sys.stderr)
            return 1
        cmd = [sys.executable, script, str(args.update_dlc)]
        if args.dry_run:
            cmd.append("--dry-run")
        if args.verbose:
            cmd.append("--verbose")
        print(f"Running: {' '.join(cmd)}")
        subprocess.run(cmd, check=True)

    # mode detection
    mode = args.mode
    if mode == "auto":
        mode = detect_mode(game_dir)
        if mode is None:
            print("error: cannot detect game type - no libsteam_api.so "
                  "(native) nor steam_api*.dll (Proton) found in "
                  f"{game_dir}", file=sys.stderr)
            return 1
        print(f"Auto-detected mode: {mode}")

    if mode == "native":
        dist = find_creamlinux_dist()
        if dist is None:
            # try the latest GitHub release of this fork
            try:
                _, url = latest_release_asset(CREAMLINUX_REPO)
                print(f"Downloading creamlinux release: {url}")
                tmpdir = tempfile.mkdtemp(prefix="creamlinux-dist-")
                zip_path = os.path.join(tmpdir, "creamlinux.zip")
                with open(zip_path, "wb") as fh:
                    fh.write(http_get(url))
                with zipfile.ZipFile(zip_path) as zf:
                    zf.extractall(tmpdir)
                dist = tmpdir
            except Exception as exc:  # noqa: BLE001
                print("error: no local creamlinux build found and no release "
                      f"available to download: {exc}", file=sys.stderr)
                print("Build it first with:  sh ./build.sh", file=sys.stderr)
                return 1
        return install_native(game_dir, dist, args.dry_run, args.verbose)

    if mode == "proton":
        cache_dir = os.path.join(os.path.expanduser("~"), ".cache", "creamlinux")
        return install_proton(game_dir, cache_dir, args.dry_run, args.verbose)

    return 1


if __name__ == "__main__":
    sys.exit(main())
