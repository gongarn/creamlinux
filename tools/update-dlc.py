#!/usr/bin/env python3
"""
update-dlc.py - fetch the DLC list for a Steam app and merge it into cream_api.ini.

The Steam Store API does not support batch appdetails lookups anymore, so DLC
names are fetched one request at a time with a small delay (rate-limit friendly).

Usage:
  python3 update-dlc.py 394360                      # merge DLCs of Hearts of Iron IV
  python3 update-dlc.py 394360 --dry-run            # show what would change
  python3 update-dlc.py 394360 --output /path/to/cream_api.ini
  python3 update-dlc.py 394360 281990 --verbose     # several games at once

The script preserves comments, section order and foreign sections; only the
[dlc] section is touched. New entries are appended, existing ids keep their
position but may get their name refreshed.
"""

import argparse
import json
import sys
import time
import urllib.request

API = "https://store.steampowered.com/api/appdetails"
CC = "us"
LANG = "english"
RETRIES = 3
SLEEP_BETWEEN_REQUESTS = 0.8  # seconds, keep Steam API happy

USER_AGENT = "creamlinux-update-dlc/1.0"


# ---------------------------------------------------------------- http ----

def http_get_json(url, retries=RETRIES):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    for attempt in range(retries):
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                raw = resp.read()
                if resp.headers.get("Content-Encoding") == "gzip":
                    import gzip
                    raw = gzip.decompress(raw)
                return json.loads(raw)
        except Exception as exc:  # noqa: BLE001 - network errors, 4xx/5xx
            if attempt == retries - 1:
                raise
            time.sleep(2 * (attempt + 1))
    return None


def fetch_app(appid):
    """Return (name, [dlc_appid...]) or (None, None) if the app is unknown."""
    data = http_get_json(
        f"{API}?appids={appid}&filters=basic&cc={CC}&l={LANG}")
    if not isinstance(data, dict):
        return None, None
    entry = data.get(str(appid))
    if not entry or not entry.get("success"):
        return None, None
    info = entry.get("data") or {}
    return info.get("name"), list(info.get("dlc") or [])


def fetch_dlc_names(dlc_ids, verbose=False):
    """Fetch names for the given DLC appids, one request at a time."""
    names = {}
    for i, dlc_id in enumerate(dlc_ids):
        if verbose:
            print(f"  [{i + 1}/{len(dlc_ids)}] fetching name for {dlc_id}...")
        try:
            name, _ = fetch_app(dlc_id)
        except Exception as exc:  # noqa: BLE001
            print(f"  warning: failed to fetch {dlc_id}: {exc}", file=sys.stderr)
            name = None
        if name:
            names[dlc_id] = name
        else:
            print(f"  warning: no name returned for DLC {dlc_id}", file=sys.stderr)
        time.sleep(SLEEP_BETWEEN_REQUESTS)
    return names


# ------------------------------------------------------------ ini model ----

class IniFile:
    """Minimal ini reader/writer that preserves comments, order and unknown
    sections. A section is a list of raw lines plus a dict view of key=value
    entries for quick updates."""

    def __init__(self):
        self.sections = []  # list of dicts: {name, lines, index_of_key}
        self.preamble = []  # lines before the first section

    @staticmethod
    def _split_key(line):
        if "=" in line:
            key, _, value = line.partition("=")
            return key.strip(), value.strip()
        return None, None

    @classmethod
    def parse(cls, text):
        ini = cls()
        current = None
        for line in text.splitlines():
            stripped = line.strip()
            if stripped.startswith("[") and stripped.endswith("]"):
                current = {"name": stripped[1:-1].strip(),
                           "lines": [], "index": {}}
                ini.sections.append(current)
            elif current is None:
                ini.preamble.append(line)
            else:
                current["lines"].append(line)
                key, _ = cls._split_key(line)
                if key is not None:
                    current["index"][key] = len(current["lines"]) - 1
        return ini

    def get_section(self, name):
        for sec in self.sections:
            if sec["name"] == name:
                return sec
        sec = {"name": name, "lines": [], "index": {}}
        self.sections.append(sec)
        return sec

    def get(self, section, key):
        sec = self.get_section(section)
        idx = sec["index"].get(key)
        if idx is None:
            return None
        _, value = self._split_key(sec["lines"][idx])
        return value

    def set(self, section, key, value):
        sec = self.get_section(section)
        idx = sec["index"].get(key)
        line = f"{key} = {value}"
        if idx is None:
            if sec["lines"] and sec["lines"][-1].strip() == "":
                sec["lines"][-1] = line
            else:
                sec["lines"].append(line)
            sec["index"][key] = len(sec["lines"]) - 1
        else:
            sec["lines"][idx] = line

    def render(self):
        out = list(self.preamble)
        for sec in self.sections:
            if out and out[-1] != "":
                out.append("")
            out.append(f"[{sec['name']}]")
            out.extend(sec["lines"])
        return "\n".join(out) + ("\n" if out else "")


# ------------------------------------------------------------- merging -----

def merge_dlcs(ini, new_dlcs, refresh_names):
    """new_dlcs: list of (appid:int, name:str) in API order.
    Returns (added, updated, skipped)."""
    added = updated = skipped = 0
    sec = ini.get_section("dlc")
    for appid, name in new_dlcs:
        key = str(appid)
        old = ini.get("dlc", key)
        if old is None:
            sec["index"][key] = len(sec["lines"])
            sec["lines"].append(f"{key} = {name}")
            added += 1
        elif refresh_names and old != name:
            idx = sec["index"][key]
            sec["lines"][idx] = f"{key} = {name}"
            updated += 1
        else:
            skipped += 1
    return added, updated, skipped


# ---------------------------------------------------------------- main -----

def main():
    ap = argparse.ArgumentParser(
        description="Fetch DLCs for Steam apps and merge them into cream_api.ini")
    ap.add_argument("appids", type=int, nargs="+",
                    help="Steam app id(s) of the game(s)")
    ap.add_argument("--output", default="cream_api.ini",
                    help="path to cream_api.ini (default: ./cream_api.ini)")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the diff without writing the file")
    ap.add_argument("--refresh-names", action="store_true",
                    help="update names of already known DLC ids")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    # 1. fetch everything
    all_dlcs = []  # (appid, name)
    for appid in args.appids:
        print(f"Fetching app {appid}...")
        try:
            name, dlc_ids = fetch_app(appid)
        except Exception as exc:  # noqa: BLE001
            print(f"error: failed to fetch app {appid}: {exc}", file=sys.stderr)
            sys.exit(1)
        if not name:
            print(f"error: app {appid} not found on Steam Store (region-locked?)",
                  file=sys.stderr)
            sys.exit(1)
        print(f"  {name}: {len(dlc_ids)} DLC(s)")
        if not dlc_ids:
            continue
        names = fetch_dlc_names(dlc_ids, verbose=args.verbose)
        all_dlcs.extend((dlc_id, names[dlc_id]) for dlc_id in dlc_ids
                        if dlc_id in names)

    if not all_dlcs:
        print("No DLC entries to merge.")
        return

    # 2. load existing ini
    try:
        with open(args.output, "r", encoding="utf-8") as fh:
            text = fh.read()
    except FileNotFoundError:
        text = ""
    ini = IniFile.parse(text)

    # 3. merge
    added, updated, skipped = merge_dlcs(ini, all_dlcs, args.refresh_names)
    print(f"Merge result: {added} added, {updated} updated, {skipped} unchanged")

    # 4. dry-run or write
    if args.dry_run:
        print("\n--- dry run: file content after merge ---")
        print(ini.render())
        return
    with open(args.output, "w", encoding="utf-8") as fh:
        fh.write(ini.render())
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
