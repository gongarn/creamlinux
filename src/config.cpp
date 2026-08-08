#include "config.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

#include "spdlog/spdlog.h"

std::vector<std::tuple<int, std::string>> dlcs;
mINI::INIStructure ini;

bool is_dlc_unlocked(AppId_t appID) {
    return std::find_if(
        std::begin(dlcs),
        std::end(dlcs),
        [&](const std::tuple<int, std::string>& a) { return std::get<0>(a) == appID; }) != std::end(dlcs);
}

bool is_unlockall() {
    return ini["config"]["unlockall"] == "true";
}

void load_config(const std::string& creaminipath) {
    spdlog::info("Reading config from {}", creaminipath);
    mINI::INIFile file(creaminipath);

    // Open ini file
    file.read(ini);
    // Find dlc's and add to vector
    for (std::pair<std::string, std::string> entry : ini["dlc"]) {
        auto dlctuple = std::make_tuple(stoi(entry.first), entry.second);
        dlcs.push_back(dlctuple);
        spdlog::info("Added dlc with id: {0}, name: {1}", entry.first, entry.second);
    }
}

std::string resolve_config_path(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return path + "/cream_api.ini";
    }
    return path;
}
