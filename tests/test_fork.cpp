// Fork-safety test (issue #56): HOI4 and other Clausewitz-engine games
// fork() without exec when restarting; the forked child then exits normally,
// which runs exit-time destructors of every static object in the child's
// copy of the process (wrapper singletons, spdlog logger, mINI state).
// A crash there surfaces to the user as "Killed"/SIGSEGV right after launch.
//
// This test creates all wrapper singletons, uses spdlog, forks, exits from
// the child (running the static destructors) and verifies that the child
// exited cleanly and the parent's singletons are still intact.
//
// Like test_vtable.cpp, it compiles steam_hooks.cpp directly so it exercises
// the real wrapper code.

#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/config.h"
#include "../src/steam_hooks.h"

#include "../spdlog/spdlog.h"
#include "../src/steam_hooks.cpp"

static int failures = 0;

#define CHECK(cond, msg)                                       \
    do {                                                       \
        if (!(cond)) {                                         \
            fprintf(stderr, "FAIL: %s\n", msg);                \
            failures++;                                        \
        }                                                      \
    } while (0)

int main() {
    // Dummy "real" pointers: the wrappers only store them at creation time
    // and must not dereference them.
    ISteamApps* apps = Hookey_SteamApps((ISteamApps*)0x1);
    ISteamUser021* user21 = Hookey_SteamUser21((ISteamUser021*)0x2);
    ISteamUser* user23 = Hookey_SteamUser23((ISteamUser*)0x3);
    ISteamClient020* client20 = Hookey_SteamClient20((ISteamClient020*)0x4);
    ISteamClient* client23 = Hookey_SteamClient23((ISteamClient*)0x5);

    CHECK(apps != nullptr && user21 != nullptr && user23 != nullptr &&
          client20 != nullptr && client23 != nullptr,
          "wrapper singleton creation failed");

    // Repeated requests must return the same instance (and must not call
    // through the dummy real pointer).
    CHECK(Hookey_SteamApps((ISteamApps*)0x1) == apps,
          "SteamApps singleton not stable");
    CHECK(Hookey_SteamUser21((ISteamUser021*)0x2) == user21,
          "SteamUser21 singleton not stable");
    CHECK(Hookey_SteamUser23((ISteamUser*)0x3) == user23,
          "SteamUser23 singleton not stable");
    CHECK(Hookey_SteamClient20((ISteamClient020*)0x4) == client20,
          "SteamClient20 singleton not stable");
    CHECK(Hookey_SteamClient23((ISteamClient*)0x5) == client23,
          "SteamClient23 singleton not stable");

    // Make sure spdlog's default logger exists before forking, so the
    // child's exit really has to run its destructor.
    spdlog::info("fork test: singletons created");

    pid_t pid = fork();
    if (pid == 0) {
        // Child: keep using the singletons and the logger, then exit
        // normally - static destructors run in this forked copy now.
        if (Hookey_SteamApps((ISteamApps*)0x1) != apps ||
            Hookey_SteamUser23((ISteamUser*)0x3) != user23 ||
            Hookey_SteamClient23((ISteamClient*)0x5) != client23) {
            _exit(42);
        }
        spdlog::info("fork test: child exiting");
        exit(0);
    }
    CHECK(pid > 0, "fork failed");
    if (pid > 0) {
        int status = 0;
        CHECK(waitpid(pid, &status, 0) == pid, "waitpid failed");
        CHECK(WIFEXITED(status),
              "child died from a signal - crash in static destructors");
        if (WIFEXITED(status)) {
            CHECK(WEXITSTATUS(status) == 0, "child exited with an error");
        }
        // Parent state must be untouched by the child's exit.
        CHECK(Hookey_SteamApps((ISteamApps*)0x1) == apps,
              "SteamApps singleton changed after fork");
        CHECK(Hookey_SteamUser21((ISteamUser021*)0x2) == user21,
              "SteamUser21 singleton changed after fork");
        CHECK(Hookey_SteamUser23((ISteamUser*)0x3) == user23,
              "SteamUser23 singleton changed after fork");
        CHECK(Hookey_SteamClient20((ISteamClient020*)0x4) == client20,
              "SteamClient20 singleton changed after fork");
        CHECK(Hookey_SteamClient23((ISteamClient*)0x5) == client23,
              "SteamClient23 singleton changed after fork");
        spdlog::info("fork test: parent alive after child exit");
    }

    if (failures == 0) {
        printf("fork safety OK\n");
        return 0;
    }
    printf("%d fork safety failure(s)\n", failures);
    return 1;
}
