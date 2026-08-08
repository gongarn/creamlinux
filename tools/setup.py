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
    if os.path.exists(os.path.join(game_dir, "steam_api64.dll")):
        return 64
    if os.path.exists(os.path.join(game_dir, "steam_api.dll")):
        return 32
    return None


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
    bitness = detect_bitness(game_dir)
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
            print(f"  extract {dll_name} -> {game_dir}/{target}")
            print("  copy cream_api.ini -> " + game_dir)
            return 0
        tmp = tempfile.mktemp(suffix=".dll")
        with open(tmp, "wb") as out:
            out.write(zf.read(dll_name))
        shutil.copy2(tmp, os.path.join(game_dir, target))
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


# ----------------------------------------------------------------- main ----

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", required=True, help="game installation directory")
    ap.add_argument("--mode", choices=["auto", "native", "proton"],
                    default="auto")
    ap.add_argument("--update-dlc", type=int, metavar="APPID",
                    help="run tools/update-dlc.py for this app first")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the plan without changing anything")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

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
