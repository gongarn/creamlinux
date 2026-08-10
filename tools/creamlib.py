"""
creamlib.py - shared core for creamlinux tools (setup.py CLI and gui.py web UI).

Contains everything related to: Steam library discovery, game type detection,
unlocker installation (creamlinux for native, SmokeAPI/Koaloader for Proton),
uninstallation, and per-game cream_api.ini editing.
"""

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
KOALOADER_REPO = "acidicoala/Koaloader"
CREAMLINUX_REPO = "gongarn/creamlinux"

CREAM_FILES = ["lib64Creamlinux.so", "lib32Creamlinux.so", "cream.sh",
               "cream_api.ini"]

DEFAULT_CACHE_DIR = os.path.join(os.path.expanduser("~"), ".cache",
                                 "creamlinux")

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(HERE)
UPDATE_DLC_SCRIPT = os.path.join(HERE, "update-dlc.py")


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

def find_steam_api_files(game_dir, max_depth=3):
    """Find libsteam_api.so / steam_api*.dll inside a game folder.
    Returns ('native', path) or ('proton', path) or (None, None).

    A libsteam_api.so wins over any steam_api*.dll: some native games
    (e.g. Paradox titles like CK3) ship stray Windows DLLs in the same
    folder, and only the .so proves a native Linux build."""
    root_depth = game_dir.rstrip(os.sep).count(os.sep)
    proton_path = None
    for dirpath, dirnames, filenames in os.walk(game_dir):
        dirnames.sort()
        filenames.sort()
        depth = dirpath.rstrip(os.sep).count(os.sep) - root_depth
        if depth >= max_depth:
            dirnames[:] = []
            continue
        for fname in filenames:
            if fname == "libsteam_api.so":
                return "native", os.path.join(dirpath, fname)
            if (fname in ("steam_api.dll", "steam_api64.dll")
                    and proton_path is None):
                proton_path = os.path.join(dirpath, fname)
        if depth >= max_depth - 1:
            dirnames[:] = []
    if proton_path is not None:
        return "proton", proton_path
    return None, None


def detect_mode(game_dir):
    found_type, _ = find_steam_api_files(game_dir)
    return found_type


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
    next to the script). Returns a dir with the files or None."""
    candidates = [
        os.path.join(REPO_ROOT, "output"),              # after build.sh
        os.path.join(REPO_ROOT, "build", "64", "lib"),  # cmake build dir
        HERE,                                            # files next to script
    ]
    for cand in candidates:
        if all(os.path.exists(os.path.join(cand, f)) for f in CREAM_FILES):
            return cand
    return None


def install_native(game_dir, dist_dir, dry_run, verbose=False, log=print):
    plan = []
    for f in CREAM_FILES:
        plan.append(("copy", os.path.join(dist_dir, f),
                     os.path.join(game_dir, f)))
    if dry_run:
        log("Native mode plan:")
        for _, src, dst in plan:
            log(f"  cp {src} -> {dst}")
        return 0
    for _, src, dst in plan:
        if verbose:
            log(f"copying {src} -> {dst}")
        shutil.copy2(src, dst)
    log("Installed creamlinux. Set the game's Steam launch options to:")
    log("  sh ./cream.sh %command%")
    log("(with MangoHud:  sh ./cream.sh mangohud %command%)")
    return 0


def uninstall_native(game_dir, remove_ini=True, log=print):
    removed = []
    files = [f for f in CREAM_FILES if f != "cream_api.ini"]
    if remove_ini:
        files.append("cream_api.ini")
    for f in files:
        path = os.path.join(game_dir, f)
        if os.path.exists(path):
            os.remove(path)
            removed.append(f)
            log(f"removed {path}")
    if not removed:
        log("No creamlinux files found - nothing to remove.")
    return 0


# --------------------------------------------------------------- proton ----

def download_smokeapi(cache_dir=None):
    cache_dir = cache_dir or DEFAULT_CACHE_DIR
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


def download_koaloader(cache_dir=None):
    cache_dir = cache_dir or DEFAULT_CACHE_DIR
    zip_path = os.path.join(cache_dir, "koaloader-latest.zip")
    if os.path.exists(zip_path):
        print(f"Using cached Koaloader: {zip_path}")
        return zip_path
    os.makedirs(cache_dir, exist_ok=True)
    print("Downloading Koaloader...")
    name, url = latest_release_asset(KOALOADER_REPO)
    data = http_get(url)
    with open(zip_path, "wb") as fh:
        fh.write(data)
    print(f"  saved {name} ({len(data)} bytes)")
    return zip_path


def _copy_from_zip(zip_path, member, dst):
    tmp = tempfile.mktemp(suffix=os.path.splitext(member)[1] or ".bin")
    with zipfile.ZipFile(zip_path) as zf:
        with open(tmp, "wb") as out:
            out.write(zf.read(member))
    shutil.copy2(tmp, dst)
    os.remove(tmp)


def install_proton(game_dir, cache_dir=None, dry_run=False, verbose=False,
                   mode="hook", log=print):
    cache_dir = cache_dir or DEFAULT_CACHE_DIR
    bitness, dll_dir = detect_bitness(game_dir)
    if bitness is None:
        log("error: no steam_api.dll / steam_api64.dll found in the game "
            "directory - SmokeAPI cannot work here")
        return 1

    # --- hook mode (default): the smoke_api dll IS the proxy dll -----------
    if mode == "hook":
        zip_path = download_smokeapi(cache_dir)
        dll_name = f"smoke_api{bitness}.dll"
        target = "version.dll" if bitness == 64 else "winhttp.dll"
        if dry_run:
            log("Proton mode plan (hook mode, self-hook):")
            log(f"  extract {dll_name} -> {dll_dir}/{target}")
            log("  copy cream_api.ini -> " + game_dir)
            return 0
        _copy_from_zip(zip_path, dll_name, os.path.join(dll_dir, target))
        log(f"Installed SmokeAPI ({bitness}-bit, hook mode) as {target}.")
        log("No launch options needed - the DLL is loaded automatically.")
        log("If the game does not load it, retry with "
            "--smokeapi-mode koaloader or --smokeapi-mode proxy.")

    # --- koaloader mode: Koaloader proxy injects smoke_api --------------
    elif mode == "koaloader":
        kzip = download_koaloader(cache_dir)
        szip = download_smokeapi(cache_dir)
        proxy_name = f"version-{bitness}/version.dll"
        smoke_name = f"smoke_api{bitness}.dll"
        target = "version.dll" if bitness == 64 else "winhttp.dll"
        if dry_run:
            log("Proton mode plan (koaloader mode):")
            log(f"  extract {proxy_name} -> {dll_dir}/{target}")
            log(f"  extract {smoke_name} -> {dll_dir}/{smoke_name}")
            log("  copy cream_api.ini -> " + game_dir)
            return 0
        _copy_from_zip(kzip, proxy_name, os.path.join(dll_dir, target))
        _copy_from_zip(szip, smoke_name, os.path.join(dll_dir, smoke_name))
        log(f"Installed Koaloader ({bitness}-bit) as {target} + {smoke_name}.")
        log("Koaloader auto-loads smoke_api from the same folder; survives "
            "game updates better than hook mode.")

    # --- proxy mode: replace steam_api(64).dll itself --------------------
    elif mode == "proxy":
        zip_path = download_smokeapi(cache_dir)
        orig = f"steam_api{bitness}.dll"
        backup = f"steam_api{bitness}_o.dll"
        smoke_name = f"smoke_api{bitness}.dll"
        if dry_run:
            log("Proton mode plan (proxy mode):")
            log(f"  rename {dll_dir}/{orig} -> {dll_dir}/{backup}")
            log(f"  extract {smoke_name} -> {dll_dir}/{orig}")
            log("  copy cream_api.ini -> " + game_dir)
            return 0
        orig_path = os.path.join(dll_dir, orig)
        backup_path = os.path.join(dll_dir, backup)
        if os.path.exists(backup_path):
            log(f"{backup} already exists, keeping it")
        elif os.path.exists(orig_path):
            shutil.move(orig_path, backup_path)
        else:
            log(f"warning: {orig} not found in {dll_dir}")
        _copy_from_zip(zip_path, smoke_name, orig_path)
        log(f"Installed SmokeAPI ({bitness}-bit, proxy mode) as {orig} "
            f"(original kept as {backup}).")
        log("Most reliable loading, but reinstall after every game update.")

    # share the DLC list
    dist = find_creamlinux_dist()
    if dist:
        shutil.copy2(os.path.join(dist, "cream_api.ini"),
                     os.path.join(game_dir, "cream_api.ini"))
        log(f"copied cream_api.ini (from {dist})")
    else:
        log("warning: no local cream_api.ini found; copy one manually "
            "into the game folder")
    return 0


def uninstall_proton(game_dir, restore_o=True, remove_ini=True, log=print):
    """Remove SmokeAPI/Koaloader files from a Proton game folder."""
    bitness, dll_dir = detect_bitness(game_dir)
    removed = []
    if bitness is not None:
        proxy_candidates = [os.path.join(dll_dir, "version.dll"),
                            os.path.join(dll_dir, "winhttp.dll")]
        # only remove proxy dlls when a smoke_api dll sits next to them,
        # so we never delete a game's own dll
        smoke = os.path.join(dll_dir, f"smoke_api{bitness}.dll")
        has_smoke = os.path.exists(smoke)
        for p in proxy_candidates:
            if has_smoke and os.path.exists(p):
                os.remove(p)
                removed.append(p)
                log(f"removed {p}")
        if os.path.exists(smoke):
            os.remove(smoke)
            removed.append(smoke)
            log(f"removed {smoke}")
        # restore the original steam_api dll if we backed it up
        orig = f"steam_api{bitness}.dll"
        backup = f"steam_api{bitness}_o.dll"
        orig_path = os.path.join(dll_dir, orig)
        backup_path = os.path.join(dll_dir, backup)
        if restore_o and os.path.exists(backup_path):
            os.remove(orig_path)
            shutil.move(backup_path, orig_path)
            log(f"restored {orig_path} from {backup_path}")
    if remove_ini:
        ini_path = os.path.join(game_dir, "cream_api.ini")
        if os.path.exists(ini_path):
            os.remove(ini_path)
            removed.append(ini_path)
            log(f"removed {ini_path}")
    if not removed:
        log("No SmokeAPI/Koaloader files found - nothing to remove.")
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


def check_dlc_files(game_dir, game_type):
    """Estimate whether the game's DLC content files are present.
    Returns 'ok' / 'partial' / 'none' / 'unknown'.
    Native games store DLC content in dlc/ (Paradox: game/dlc) or DLC/
    folders; for Proton games the layout is unpredictable -> 'unknown'."""
    if game_type != "native":
        return "unknown"
    for folder in ("dlc", "DLC", "dlc_metadata",
                   os.path.join("game", "dlc"), os.path.join("game", "DLC")):
        path = os.path.join(game_dir, folder)
        if os.path.isdir(path):
            try:
                count = len([e for e in os.listdir(path)
                             if not e.startswith(".")])
            except OSError:
                count = 0
            if count >= 10:
                return "ok"
            if count > 0:
                return "partial"
    return "none"


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
                _, dll_dir = detect_bitness(game_dir)
                if dll_dir and (os.path.exists(
                        os.path.join(dll_dir, "version.dll")) or
                        os.path.exists(os.path.join(dll_dir, "winhttp.dll"))):
                    installed = "smokeapi"
            games.append({"appid": appid, "name": name, "game_dir": game_dir,
                          "game_type": game_type, "installed": installed,
                          "dlc_status": check_dlc_files(game_dir, game_type)})
    return games


def find_game(appid):
    """Return the game dict for an appid from the scanned libraries."""
    for game in scan_games():
        if game["appid"] == str(appid):
            return game
    return None


# ------------------------------------------------------- cream_api.ini -----

def game_ini_path(game_dir):
    return os.path.join(game_dir, "cream_api.ini")


def read_game_ini(game_dir):
    """Return {'config': {...}, 'methods': {...}, 'dlc': [[id, name], ...]}
    or None if the game has no cream_api.ini."""
    path = game_ini_path(game_dir)
    if not os.path.exists(path):
        return None
    result = {"config": {}, "methods": {}, "dlc": []}
    section = None
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.strip()
            if line.startswith("[") and line.endswith("]"):
                section = line[1:-1].strip()
                if section not in result:
                    result[section] = {}
            elif "=" in line and section is not None:
                key, _, value = line.partition("=")
                key, value = key.strip(), value.strip()
                if section == "dlc":
                    result["dlc"].append([key, value])
                else:
                    result[section][key] = value
    return result


def write_game_ini(game_dir, config, methods, dlc_entries, log=print):
    """Write cream_api.ini into the game folder. config/methods: dicts,
    dlc_entries: list of [appid, name]."""
    path = game_ini_path(game_dir)
    lines = ["# generated by creamlinux tools",
             "[config]"]
    for k, v in config.items():
        lines.append(f"{k} = {v}")
    lines.append("")
    lines.append("[methods]")
    for k, v in methods.items():
        lines.append(f"{k} = {v}")
    lines.append("")
    lines.append("[dlc]")
    for appid, name in dlc_entries:
        lines.append(f"{appid} = {name}")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    log(f"wrote {path} ({len(dlc_entries)} DLC entries)")


# ------------------------------------------------------------ dlc update ---

def run_dlc_update(appid, dry_run=False, log=print):
    """Run tools/update-dlc.py for an appid, streaming output to log()."""
    if not os.path.exists(UPDATE_DLC_SCRIPT):
        log(f"error: {UPDATE_DLC_SCRIPT} not found")
        return 1
    cmd = [sys.executable, UPDATE_DLC_SCRIPT, str(appid)]
    if dry_run:
        cmd.append("--dry-run")
    log(f"Running: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    assert proc.stdout is not None
    for line in proc.stdout:
        log(line.rstrip("\n"))
    proc.wait()
    return proc.returncode
