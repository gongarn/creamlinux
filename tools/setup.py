#!/usr/bin/env python3
"""
setup.py - install DLC unlockers into a game folder (CLI).

One tool for both cases:
  * native Linux games      -> creamlinux  (LD_PRELOAD hooks)
  * Windows games in Proton -> SmokeAPI    (DLL proxy/hook, by acidicoala)

The cream_api.ini DLC list is shared between both unlockers.

Usage:
  python3 tools/setup.py --dir /path/to/game                  # auto-detect mode
  python3 tools/setup.py --dir /path/to/game --mode native
  python3 tools/setup.py --dir /path/to/game --mode proton
  python3 tools/setup.py --dir /path/to/game --smokeapi-mode koaloader
  python3 tools/setup.py --scan                               # list installed Steam games
  python3 tools/setup.py --install 394360                     # install unlocker for an appid
  python3 tools/setup.py --install 394360 --dry-run           # preview only

All logic lives in creamlib.py (shared with gui.py).
"""

import argparse
import os
import subprocess
import sys
import tempfile
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import creamlib  # noqa: E402


def cmd_scan(args):
    games = creamlib.scan_games()
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
    game = creamlib.find_game(args.install)
    if not game:
        print(f"error: app {args.install} not found in Steam libraries",
              file=sys.stderr)
        return 1
    print(f"Installing unlocker for {game['name']} "
          f"({game['game_type']}, {game['game_dir']})")
    if game["game_type"] == "native":
        dist = creamlib.find_creamlinux_dist()
        if dist is None:
            print("error: no local creamlinux build found; run 'sh ./build.sh' "
                  "first or pass --dir with a prebuilt dist", file=sys.stderr)
            return 1
        return creamlib.install_native(game["game_dir"], dist,
                                       args.dry_run, args.verbose)
    if game["game_type"] == "proton":
        return creamlib.install_proton(game["game_dir"], dry_run=args.dry_run,
                                       verbose=args.verbose,
                                       mode=args.smokeapi_mode)
    print(f"error: {game['name']} has no Steam API files "
          "(libsteam_api.so / steam_api*.dll)", file=sys.stderr)
    return 1


def cmd_uninstall(args):
    game = creamlib.find_game(args.install)
    if not game:
        print(f"error: app {args.install} not found in Steam libraries",
              file=sys.stderr)
        return 1
    print(f"Removing unlocker from {game['name']} ({game['game_dir']})")
    if game["game_type"] == "native":
        return creamlib.uninstall_native(game["game_dir"],
                                         remove_ini=not args.keep_ini)
    if game["game_type"] == "proton":
        return creamlib.uninstall_proton(game["game_dir"],
                                         remove_ini=not args.keep_ini)
    print("Nothing to uninstall.")
    return 0


def main():
    ap = argparse.ArgumentParser(
        description="Install/remove DLC unlockers for Steam games",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", help="game installation directory")
    ap.add_argument("--mode", choices=["auto", "native", "proton"],
                    default="auto")
    ap.add_argument("--smokeapi-mode", choices=["hook", "koaloader", "proxy"],
                    default="hook",
                    help="Proton install mode: hook (self-hook, default), "
                         "koaloader (injector proxy) or proxy (replace "
                         "steam_api dll)")
    ap.add_argument("--scan", action="store_true",
                    help="list installed Steam games and their unlocker status")
    ap.add_argument("--install", type=int, metavar="APPID",
                    help="install the unlocker for an installed Steam game")
    ap.add_argument("--uninstall", type=int, metavar="APPID",
                    help="remove the unlocker from an installed Steam game")
    ap.add_argument("--keep-ini", action="store_true",
                    help="keep cream_api.ini when uninstalling")
    ap.add_argument("--update-dlc", type=int, metavar="APPID",
                    help="refresh cream_api.ini DLC list for an appid")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the plan without changing anything")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if args.scan:
        return cmd_scan(args)
    if args.install is not None:
        return cmd_install(args)
    if args.uninstall is not None:
        return cmd_uninstall(args)
    if args.update_dlc is not None:
        return creamlib.run_dlc_update(args.update_dlc,
                                       dry_run=args.dry_run,
                                       log=print)
    if not args.dir:
        ap.error("one of --dir, --scan, --install, --uninstall or "
                 "--update-dlc is required")

    game_dir = os.path.abspath(args.dir)
    if not os.path.isdir(game_dir):
        print(f"error: {game_dir} is not a directory", file=sys.stderr)
        return 1

    # optional: refresh the DLC list first
    if args.update_dlc:
        rc = creamlib.run_dlc_update(args.update_dlc, dry_run=args.dry_run,
                                     log=print)
        if rc != 0:
            return rc

    # mode detection
    mode = args.mode
    if mode == "auto":
        mode = creamlib.detect_mode(game_dir)
        if mode is None:
            print("error: cannot detect game type - no libsteam_api.so "
                  "(native) nor steam_api*.dll (Proton) found in "
                  f"{game_dir}", file=sys.stderr)
            return 1
        print(f"Auto-detected mode: {mode}")

    if mode == "native":
        dist = creamlib.find_creamlinux_dist()
        if dist is None:
            # try the latest GitHub release of this fork
            try:
                _, url = creamlib.latest_release_asset(creamlib.CREAMLINUX_REPO)
                print(f"Downloading creamlinux release: {url}")
                tmpdir = tempfile.mkdtemp(prefix="creamlinux-dist-")
                zip_path = os.path.join(tmpdir, "creamlinux.zip")
                with open(zip_path, "wb") as fh:
                    fh.write(creamlib.http_get(url))
                with zipfile.ZipFile(zip_path) as zf:
                    zf.extractall(tmpdir)
                dist = tmpdir
            except Exception as exc:  # noqa: BLE001
                print("error: no local creamlinux build found and no release "
                      f"available to download: {exc}", file=sys.stderr)
                print("Build it first with:  sh ./build.sh", file=sys.stderr)
                return 1
        return creamlib.install_native(game_dir, dist, args.dry_run,
                                       args.verbose)

    if mode == "proton":
        return creamlib.install_proton(game_dir, dry_run=args.dry_run,
                                       verbose=args.verbose,
                                       mode=args.smokeapi_mode)

    return 1


if __name__ == "__main__":
    sys.exit(main())
