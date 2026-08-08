#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "ext/ini.h"
#include "ext/steam/steamtypes.h"

// DLC list loaded from cream_api.ini ([dlc] section: "<appid> = <name>").
extern std::vector<std::tuple<int, std::string>> dlcs;
// Parsed cream_api.ini structure (also holds [methods]/[config] toggles).
extern mINI::INIStructure ini;

// Returns true if the given appID is present in the cream_api.ini [dlc] list.
bool is_dlc_unlocked(AppId_t appID);

// Returns true when [config] unlockall is enabled: every DLC the game knows
// about is treated as installed, no ID list required.
bool is_unlockall();

// Reads the ini file and fills the global dlcs vector.
void load_config(const std::string& creaminipath);

// Resolves a user-supplied CREAM_CONFIG_PATH (file or directory) to the ini
// file path: appends /cream_api.ini when the path is a directory.
std::string resolve_config_path(const std::string& path);
