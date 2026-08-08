// Export hooks: dlsym interposition, SteamAPI_Init, SteamInternal_* interface
// creation and the flat API hooks. This file is the "front door" of the .so:
// everything here is either an exported symbol games resolve through
// LD_PRELOAD or a helper for those hooks.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <string>
#include <sys/stat.h>

#include "config.h"
#include "interface_versions.h"
#include "spdlog/spdlog.h"
#include "steam_hooks.h"

#include "ext/steam/isteamapps.h"
#include "ext/steam/isteamclient.h"
#include "ext/steam/isteamclient020.h"
#include "ext/steam/isteamuser.h"
#include "ext/steam/isteamuser021.h"
#include "ext/steam/steam_api_common.h"

using namespace std;

void* (*real_dlsym)(void *handle, const char *name);

//TODO: hook dlvsym as well
void ensure_realdlsym() {
    if (real_dlsym == NULL) {
        *(void **)(&real_dlsym) = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
    }
}

extern "C" void* CreateInterface(const char *pName, int *pReturnCode) {
    ensure_realdlsym();
    void *S_CALLTYPE (*real)(const char *pName, int *pReturnCode);
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "CreateInterface");
    spdlog::info("CreateInterface called pszVersion: {}", pName);
    return real(pName, pReturnCode);
}

// ---- Flat API hooks (steam_api_flat.h) ----
// Some games (e.g. Unity titles) call SteamAPI_ISteamApps_* and SteamAPI_ISteamUser_*
// directly instead of going through the C++ interface accessors. Since creamlinux is
// LD_PRELOADed before libsteam_api.so, these symbols interpose the real ones.
// NOTE: the real flat API passes `self` as the first argument; it must stay in the
// signature or 32-bit builds will read the wrong stack slot.

extern "C" int SteamAPI_ISteamApps_GetDLCCount(ISteamApps* self) {
    spdlog::info("SteamAPI_ISteamApps_GetDLCCount called (flat API)");
    if (is_unlockall()) {
        return self->GetDLCCount();
    }
    return (int)dlcs.size();
}

extern "C" bool SteamAPI_ISteamApps_BIsDlcInstalled(ISteamApps* self, AppId_t appID) {
    spdlog::info("SteamAPI_ISteamApps_BIsDlcInstalled called (flat API) appID {}", appID);
    if (is_unlockall()) {
        spdlog::info("SteamAPI_ISteamApps_BIsDlcInstalled unlockall: unlocked {}", appID);
        return true;
    }
    if (is_dlc_unlocked(appID)) {
        spdlog::info("SteamAPI_ISteamApps_BIsDlcInstalled unlocked {}", appID);
        return true;
    }
    return false;
}

extern "C" bool SteamAPI_ISteamApps_BIsSubscribedApp(ISteamApps* self, AppId_t appID) {
    spdlog::info("SteamAPI_ISteamApps_BIsSubscribedApp called (flat API) appID {}", appID);
    if (ini["methods"]["disable_steamapps_issubscribedapp"] == "true") {
        return self->BIsSubscribedApp(appID);
    }
    if (is_unlockall()) {
        spdlog::info("SteamAPI_ISteamApps_BIsSubscribedApp unlockall: unlocked {}", appID);
        return true;
    }
    if (is_dlc_unlocked(appID)) {
        spdlog::info("SteamAPI_ISteamApps_BIsSubscribedApp unlocked {}", appID);
        return true;
    }
    return false;
}

extern "C" bool SteamAPI_ISteamApps_BGetDLCDataByIndex(ISteamApps* self, int iDLC, AppId_t* pAppID, bool* pbAvailable, char* pchName, int cchNameBufferSize) {
    spdlog::info("SteamAPI_ISteamApps_BGetDLCDataByIndex called (flat API)");
    if (is_unlockall()) {
        return self->BGetDLCDataByIndex(iDLC, pAppID, pbAvailable, pchName, cchNameBufferSize);
    }
    if ((size_t)iDLC >= dlcs.size()) {
        return false;
    }

    *pAppID = std::get<0>(dlcs[iDLC]);
    *pbAvailable = true;

    const char* name = std::get<1>(dlcs[iDLC]).c_str();
    size_t slen = std::min((size_t)cchNameBufferSize - 1, std::get<1>(dlcs[iDLC]).size());
    memcpy((void*)pchName, (void*)name, slen);
    *(pchName + slen) = 0x0;

    return true;
}

extern "C" EUserHasLicenseForAppResult SteamAPI_ISteamUser_UserHasLicenseForApp(ISteamUser* self, uint64 steamID, AppId_t appID) {
    spdlog::info("SteamAPI_ISteamUser_UserHasLicenseForApp called (flat API) appID {}", appID);
    if (is_unlockall() || is_dlc_unlocked(appID)) {
        spdlog::info("SteamAPI_ISteamUser_UserHasLicenseForApp result: owned");
        return (EUserHasLicenseForAppResult)0;
    }
    spdlog::info("SteamAPI_ISteamUser_UserHasLicenseForApp result: not owned");
    return (EUserHasLicenseForAppResult)2;
}

extern "C" void* S_CALLTYPE SteamInternal_FindOrCreateUserInterface(HSteamUser hSteamUser, const char *pszVersion) {
    ensure_realdlsym();
    void* S_CALLTYPE (*real)(HSteamUser hSteamUser, const char *pszVersion);
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamInternal_FindOrCreateUserInterface");
    spdlog::info("SteamInternal_FindOrCreateUserInterface called pszVersion: {}", pszVersion);
    // Steamapps Interface call is hooked here (v008 and v009 share a vtable prefix, one wrapper is enough)
    if (strstr(pszVersion, STEAMAPPS_INTERFACE_VERSION_N008) == pszVersion ||
        strstr(pszVersion, STEAMAPPS_INTERFACE_VERSION_N009) == pszVersion) {
        ISteamApps* val = (ISteamApps*)real(hSteamUser, pszVersion);
        spdlog::info("SteamInternal_FindOrCreateUserInterface hooked ISteamApps {}", pszVersion);
        return Hookey_SteamApps(val);
    }

    // SteamUser v022/v023 (GetAuthTicketForWebApi shifted the vtable)
    if (strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_022) == pszVersion ||
        strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_023) == pszVersion) {
        ISteamUser* val = (ISteamUser*)real(hSteamUser, pszVersion);
        spdlog::info("SteamInternal_FindOrCreateUserInterface ISteamUser hook {}", pszVersion);
        return Hookey_SteamUser23(val);
    }
    // SteamUser v020/v021
    if (strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_020) == pszVersion ||
        strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_021) == pszVersion) {
        ISteamUser021* val = (ISteamUser021*)real(hSteamUser, pszVersion);
        spdlog::info("SteamInternal_FindOrCreateUserInterface ISteamUser(legacy) hook {}", pszVersion);
        return Hookey_SteamUser21(val);
    }
    auto val = real(hSteamUser, pszVersion);
    return val;
}

extern "C" void* S_CALLTYPE SteamInternal_CreateInterface(const char *pszVersion) {
    ensure_realdlsym();
     void* S_CALLTYPE (*real)(const char *pszVersion);
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamInternal_CreateInterface");
    spdlog::info("SteamInternal_CreateInterface called pszVersion: {}", pszVersion);

    // Steamapps Interface call is hooked here (v008 and v009 share a vtable prefix, one wrapper is enough)
    if (strstr(pszVersion, STEAMAPPS_INTERFACE_VERSION_N008) == pszVersion ||
        strstr(pszVersion, STEAMAPPS_INTERFACE_VERSION_N009) == pszVersion) {
        ISteamApps* val = (ISteamApps*)real(pszVersion);
        spdlog::info("SteamInternal_CreateInterface hooked ISteamApps {}", pszVersion);
        return Hookey_SteamApps(val);
    }

    // SteamUser v022/v023 (GetAuthTicketForWebApi shifted the vtable)
    if (strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_022) == pszVersion ||
        strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_023) == pszVersion) {
        ISteamUser* val = (ISteamUser*)real(pszVersion);
        spdlog::info("SteamInternal_CreateInterface ISteamUser hook {}", pszVersion);
        return Hookey_SteamUser23(val);
    }

    // SteamUser v020/v021
    if (strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_020) == pszVersion ||
        strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_021) == pszVersion) {
        ISteamUser021* val = (ISteamUser021*)real(pszVersion);
        spdlog::info("SteamInternal_CreateInterface ISteamUser(legacy) hook {}", pszVersion);
        return Hookey_SteamUser21(val);
    }

    // SteamClient v021/v022/v023
    if (strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_021) == pszVersion ||
        strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_022) == pszVersion ||
        strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_023) == pszVersion) {
        ISteamClient* val = (ISteamClient*)real(pszVersion);
        spdlog::info("SteamInternal_CreateInterface ISteamClient hook {}", pszVersion);
        return Hookey_SteamClient23(val);
    }

    // SteamClient v017-v020 (legacy, vtable compatible)
    if (strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_017) == pszVersion ||
        strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_018) == pszVersion ||
        strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_019) == pszVersion ||
        strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_020) == pszVersion) {
        ISteamClient020* val = (ISteamClient020*)real(pszVersion);
        spdlog::info("SteamInternal_CreateInterface ISteamClient(legacy) hook {}", pszVersion);
        return Hookey_SteamClient20(val);
    }

    auto val = real(pszVersion);
    return val;
}

// for older games
// NOTE: the v009/v023 headers define inline accessors with these names; we export our
// own symbols instead so old binaries that link SteamApps()/SteamUser() directly get hooked.
extern "C" ISteamApps *S_CALLTYPE SteamApps() {
    ensure_realdlsym();
    spdlog::info("SteamApps() called");

    //get isteamapps
    void* S_CALLTYPE (*real)();
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamApps");
    ISteamApps* val = (ISteamApps*)real();
    //return val;
    return Hookey_SteamApps(val);
}
// for older games
extern "C" ISteamUser *S_CALLTYPE SteamUser() {
    ensure_realdlsym();
    spdlog::info("SteamUser() called");

    //get isteamuser
    void* S_CALLTYPE (*real)();
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamUser");
    ISteamUser* val = (ISteamUser*)real();
    //return val;
    // legacy accessor: games on old SDKs expect the SteamUser020/021 vtable
    return (ISteamUser*)Hookey_SteamUser21((ISteamUser021*)val);
}

// TODO: SteamClient() accessor hook is intentionally disabled: hooking it broke
// PAYDAY 2 (Paradox launcher shows 'Not owned'). Revisit once the wrappers can
// dispatch on the requested SDK version.

// Used by SteamAPI_Init() to list loaded libraries when dlsym resolution fails.
static int printdliter(struct dl_phdr_info *info, size_t size, void *data) {
  printf("%s\n", info->dlpi_name);
  return 0;
}

extern "C" bool SteamAPI_Init()
{
    ensure_realdlsym();
    std::string creaminipath = "cream_api.ini";
    // for developers: enable these 2 lines if you want to log output to a file
    // auto logger = std::make_shared<spdlog::sinks::basic_file_sink_mt>("creamlinux_log.txt", true);
    // spdlog::default_logger().get()->sinks().push_back(logger);

    auto env = std::getenv("CREAM_CONFIG_PATH");
    //f env exists, use it
    if (env != NULL) {
        // Accept both a path to the ini file and a directory containing cream_api.ini
        creaminipath = resolve_config_path(env);
    }

    load_config(creaminipath);
    spdlog::info("SteamAPI_Init called in PID {0}", getpid());

    // finish api call
    // the spaghetti below this comment is calling the original Init function
    // can probably be simplified but i'm no c++ expert
    bool (*real)();
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamAPI_Init");
    char* errstr = dlerror();
    if (errstr != NULL) {
        spdlog::error("SteamAPI_Init failed; A dynamic linking error occurred: {0}", errstr);
        spdlog::error("Listing open libraries.");
        dl_iterate_phdr(printdliter, NULL);
        return false;
    }
    spdlog::info("Calling real SteamAPI_Init");
    auto retval = real();
    spdlog::info("SteamAPI_Init returned {0}", retval);

    return retval;
}

//TODO: add a flag to allow users to disable the dlsym hooking method
extern "C" void *dlsym(void *handle, const char *name)
{
    // filelog->info("modified dlsym called with {0}", name); // enable dlsym logging for quirky use cases (Proton, mono apps)

    ensure_realdlsym();

    /* my target binary is even asking for dlsym() via dlsym()... */
    if (!strcmp(name,"dlsym"))
        return (void*)dlsym;

    if (!strcmp(name, "SteamAPI_Init") && handle != RTLD_NEXT) {
        spdlog::info("returning custom impl for {0}", name);
        spdlog::info("custom: {0}, real: {1}", (void *)SteamAPI_Init, real_dlsym(RTLD_NEXT, "SteamAPI_Init"));
        return (void *)SteamAPI_Init;
    }
    if (!strcmp(name, "SteamInternal_CreateInterface") && handle != RTLD_NEXT) {
        spdlog::info("returning custom impl for {0}", name);
        spdlog::info("custom: {0}, real: {1}", (void *)SteamInternal_CreateInterface, real_dlsym(RTLD_NEXT, "SteamInternal_CreateInterface"));
        return (void *)SteamInternal_CreateInterface;
    }
    // enabling this makes some games (golf with your friends) start calling non-existing functions
    //TODO: add a setting to let users use this as a hooking method
    // if (!strcmp(name, "CreateInterface") && handle != RTLD_NEXT) {
    //     filelog->info("returning custom impl for {0}", name);
    //     filelog->info("custom: {0}, real: {1}", (void *)CreateInterface, real_dlsym(RTLD_NEXT, "CreateInterface"));
    //     return (void *)CreateInterface;
    // }

    return real_dlsym(handle,name);
}
