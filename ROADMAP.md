# Roadmap

Vision: an up-to-date, self-sustaining creamlinux fork for native Linux games,
plus a single install tool covering both native and Proton titles.

## ✅ Done

| # | Item | Notes |
|---|------|-------|
| 1 | Modern Steam interfaces | SteamApps v009, SteamUser v020-023, SteamClient v017-023 (SDK 1.65 headers, separate vtable wrappers), flat API hooks (Unity/Steamworks.NET), 32-bit flat API fix |
| 2 | CI + auto-sync | checkout@v4, gh release CLI, ubuntu:24.04, weekly cream_api.ini sync from upstream |
| 3 | Refactor | main.cpp (32 KB) split into src/{config, steam_hooks, export_hooks}, dead code removed |
| 4 | Fixes | cream.sh works from any CWD (#62), MangoHud support (#51), README |
| 5 | CI green | apt mirror workaround, submodule step dropped |
| 6 | CREAM_CONFIG_PATH | accepts file or directory, resolve_config_path() |
| 7 | Unit tests | config module, wired into ctest + CI |
| 8 | tools/update-dlc.py | DLC list from Steam Store API, preserves ini comments/sections |
| 9 | tools/setup.py | auto-detect native/Proton, SmokeAPI install, shared cream_api.ini |
| 10 | setup.py --scan/--install | Steam library discovery (libraryfolders.vdf + appmanifest_*.acf), table view, per-appid install, Steam API files found in subdirs (bin/x64, lib) |
| 11 | unlockall = true | every DLC the game knows about is treated as installed - no ID list needed; interface + flat API hooks | CreamAPI |
| 12 | setup.py --smokeapi-mode hook\|koaloader\|proxy | Proton fallback install modes; Koaloader injector proxy | Koaloader |
| 13 | cream.sh bitness check | preload only the matching lib32/lib64, clean logs (#71) | CreamAPI |
| 14 | logging = false | [config] flag silences spdlog output | CreamAPI |
| 15 | Web UI | tools/gui.py: local web app (games table, install/uninstall, config editor, DLC updater, live log); creamlib.py shared core; setup.py CLI regression-tested | - |
| 16 | Release v2.3.1 | CI-created release with creamlinux.zip (gh release, contents:write fix) |
| 17 | GUI: DLC-file status column | ok/partial/none/unknown per game with tooltips |
| 18 | GUI: multi-select batch install/uninstall | appids arrays, per-game log headers |
| 19 | Python unit tests (12) + CI step | creamlib: VDF, scan, install/uninstall, ini |
| 20 | vtable layout test | wrapper overrides checked against base slots (Itanium ABI) |
| 21 | ContextInit hook + opt-in SteamClient() | dlvsym deliberately not interposed (bootstrap recursion) |
| 22 | GitHub Pages landing | gongarn.github.io/creamlinux |

## 📋 Backlog

- PR to upstream: cream.sh fixes (opened: anticitizn/creamlinux#75);
  interface-fixes PR needs adaptation to upstream's main.cpp layout
- Real-game testing: CK3 (#71), Dead Cells (#29), MangoHud (#51),
  Proton path (SmokeAPI) - HOI4 already confirmed working
- README: tested-games table from issues
- Uplay/Epic support - intentionally out of scope (Steam only)

## ⚠️ Risks

- No real-game testing yet - the biggest risk (vtable wrappers and flat API
  verified by compilation only)
- Steam SDK moves on - interfaces will age (monitor Proton branches)
- SmokeAPI/Koaloader are external dependencies (their releases change)
