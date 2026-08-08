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
## 📋 Backlog

- Hooks for new init paths: SteamInternal_ContextInit, dlvsym, SteamClient()
  accessor behind an ini flag (disabled due to PAYDAY 2)
- Release v2.3.0 with zip artifact (CI ready; setup.py can then download it)
- Real-game testing: **HOI4 confirmed working** (native, unlockall,
  installed via web UI); still to verify: CK3 (#71), Dead Cells (#29),
  MangoHud (#51), Proton path (SmokeAPI)
- PR back to anticitizn/creamlinux (interface fixes)
- README: tested-games table from issues
- Uplay/Epic support - intentionally out of scope (Steam only)

## ⚠️ Risks

- No real-game testing yet - the biggest risk (vtable wrappers and flat API
  verified by compilation only)
- Steam SDK moves on - interfaces will age (monitor Proton branches)
- SmokeAPI/Koaloader are external dependencies (their releases change)
