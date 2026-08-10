// Flat API test (issue #60): Unity games using Steamworks.NET (Battletech,
// Rimworld, Dead Cells) call SteamAPI_ISteamApps_* / SteamAPI_ISteamUser_*
// directly instead of the C++ interface accessors, and Mono-based
// Steamworks.NET resolves them through dlsym(handle) on the already loaded
// libsteam_api.so - bypassing LD_PRELOAD symbol interposition. Two things
// must hold:
//   1. the flat hooks answer from the cream_api.ini [dlc] list without
//      dereferencing `self` (in list-based mode the real ISteamApps pointer
//      is not needed at all);
//   2. our dlsym() hook returns these implementations for any handle
//      except RTLD_NEXT, so Steamworks.NET hits the unlock logic too.
//
// Compiles export_hooks.cpp + steam_hooks.cpp + config.cpp directly, like
// test_vtable.cpp does for the wrapper code.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "../src/config.h"

#include "../src/export_hooks.cpp"
#include "../src/steam_hooks.cpp"

static int failures = 0;

#define CHECK(cond, msg)                                       \
    do {                                                       \
        if (!(cond)) {                                         \
            fprintf(stderr, "FAIL: %s\n", msg);                \
            failures++;                                        \
        }                                                      \
    } while (0)

// Writes a temp cream_api.ini and returns its path.
static std::string write_ini(const char *body) {
    char tmpl[] = "/tmp/creamflat_XXXXXX";
    int fd = mkstemp(tmpl);
    CHECK(fd >= 0, "mkstemp failed");
    ssize_t n = write(fd, body, strlen(body));
    CHECK(n == (ssize_t)strlen(body), "write ini failed");
    close(fd);
    return std::string(tmpl);
}

int main() {
    // ---- list-based mode: [dlc] drives the answers, self is unused ----
    std::string ini_path = write_ini(
        "[config]\n"
        "unlockall = false\n"
        "\n"
        "[dlc]\n"
        "12345 = Test DLC\n");
    load_config(ini_path);

    ISteamApps *fake_apps = (ISteamApps *)0x1;
    CHECK(SteamAPI_ISteamApps_GetDLCCount(fake_apps) == 1,
          "GetDLCCount must return the [dlc] list size");
    CHECK(SteamAPI_ISteamApps_BIsSubscribedApp(fake_apps, 12345) == true,
          "BIsSubscribedApp must unlock ids from the [dlc] list");
    CHECK(SteamAPI_ISteamApps_BIsSubscribedApp(fake_apps, 99999) == false,
          "BIsSubscribedApp must reject ids not in the list");
    CHECK(SteamAPI_ISteamApps_BIsDlcInstalled(fake_apps, 12345) == true,
          "BIsDlcInstalled must unlock ids from the [dlc] list");
    CHECK(SteamAPI_ISteamApps_BIsDlcInstalled(fake_apps, 99999) == false,
          "BIsDlcInstalled must reject ids not in the list");

    AppId_t pid = 0;
    bool avail = false;
    char name[64] = {0};
    CHECK(SteamAPI_ISteamApps_BGetDLCDataByIndex(
              fake_apps, 0, &pid, &avail, name, sizeof(name)) == true,
          "BGetDLCDataByIndex must succeed for index 0");
    CHECK(pid == 12345 && avail == true,
          "BGetDLCDataByIndex must return the [dlc] appid/availability");
    CHECK(strcmp(name, "Test DLC") == 0,
          "BGetDLCDataByIndex must copy the [dlc] name");

    ISteamUser *fake_user = (ISteamUser *)0x2;
    CHECK(SteamAPI_ISteamUser_UserHasLicenseForApp(fake_user, 0, 12345) == 0,
          "UserHasLicenseForApp must report owned for [dlc] ids");

    // ---- dlsym must hand out the flat hooks (Steamworks.NET path) ----
    CHECK(dlsym(RTLD_DEFAULT, "SteamAPI_Init") == (void *)SteamAPI_Init,
          "dlsym must return our SteamAPI_Init hook");
    CHECK(dlsym(RTLD_DEFAULT, "SteamAPI_ISteamApps_BIsSubscribedApp") ==
              (void *)SteamAPI_ISteamApps_BIsSubscribedApp,
          "dlsym must return our BIsSubscribedApp flat hook");
    CHECK(dlsym(RTLD_DEFAULT, "SteamAPI_ISteamApps_GetDLCCount") ==
              (void *)SteamAPI_ISteamApps_GetDLCCount,
          "dlsym must return our GetDLCCount flat hook");
    CHECK(dlsym(RTLD_DEFAULT, "SteamAPI_ISteamApps_BIsDlcInstalled") ==
              (void *)SteamAPI_ISteamApps_BIsDlcInstalled,
          "dlsym must return our BIsDlcInstalled flat hook");
    CHECK(dlsym(RTLD_DEFAULT, "SteamAPI_ISteamApps_BGetDLCDataByIndex") ==
              (void *)SteamAPI_ISteamApps_BGetDLCDataByIndex,
          "dlsym must return our BGetDLCDataByIndex flat hook");
    CHECK(dlsym(RTLD_DEFAULT, "SteamAPI_ISteamUser_UserHasLicenseForApp") ==
              (void *)SteamAPI_ISteamUser_UserHasLicenseForApp,
          "dlsym must return our UserHasLicenseForApp flat hook");
    // unrelated names must not be hijacked
    CHECK(dlsym(RTLD_DEFAULT, "SteamAPI_ISteamApps_GetCurrentGameLanguage") ==
              NULL,
          "dlsym must not hijack unrelated flat API names");

    // ---- unlockall mode: everything is unlocked; self is still unused
    // ---- for the calls that do not delegate to the real interface
    dlcs.clear();
    ini.clear();
    std::string ini2 = write_ini("[config]\nunlockall = true\n\n[dlc]\n");
    load_config(ini2);

    CHECK(SteamAPI_ISteamApps_BIsSubscribedApp(fake_apps, 99999) == true,
          "unlockall: BIsSubscribedApp must unlock any id");
    CHECK(SteamAPI_ISteamApps_BIsDlcInstalled(fake_apps, 99999) == true,
          "unlockall: BIsDlcInstalled must unlock any id");
    CHECK(SteamAPI_ISteamUser_UserHasLicenseForApp(fake_user, 0, 99999) == 0,
          "unlockall: UserHasLicenseForApp must report owned");

    unlink(ini_path.c_str());
    unlink(ini2.c_str());

    if (failures == 0) {
        printf("flat API OK\n");
        return 0;
    }
    printf("%d flat API failure(s)\n", failures);
    return 1;
}
