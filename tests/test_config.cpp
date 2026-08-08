// Unit tests for src/config.cpp (ini loading, DLC list, path resolution).
// No external test framework: a plain CHECK macro keeps the dependency count
// at zero. Run via ctest or directly.

#include <cstdio>
#include <cstdlib>
#include <string>

#include "../src/config.h"

static int failures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAIL: %s (line %d): %s\n", __FILE__, __LINE__,    \
                    msg);                                                      \
            failures++;                                                        \
        } else {                                                               \
            printf("ok: %s\n", msg);                                           \
        }                                                                      \
    } while (0)

static const char* kTestIni = "/tmp/creamlinux_test_config.ini";

static void write_test_ini() {
    FILE* f = fopen(kTestIni, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", kTestIni);
        exit(1);
    }
    fprintf(f, "# test comment\n");
    fprintf(f, "[dlc]\n");
    fprintf(f, "394360 = Hearts of Iron IV\n");
    fprintf(f, "281990 = Stellaris\n");
    fprintf(f, "1158310 = Crusader Kings III\n");
    fprintf(f, "\n");
    fprintf(f, "[methods]\n");
    fprintf(f, "disable_steamapps_issubscribedapp = true\n");
    fprintf(f, "\n");
    fprintf(f, "[config]\n");
    fprintf(f, "issubscribedapp_on_false_use_real = true\n");
    fclose(f);
}

int main() {
    write_test_ini();

    // --- load_config -------------------------------------------------------
    load_config(kTestIni);
    CHECK(dlcs.size() == 3, "load_config parses three DLC entries");
    CHECK(std::get<0>(dlcs[0]) == 394360, "first DLC id is 394360");
    CHECK(std::get<1>(dlcs[0]) == "Hearts of Iron IV", "first DLC name parsed");
    CHECK(std::get<0>(dlcs[1]) == 281990, "second DLC id is 281990");
    CHECK(std::get<0>(dlcs[2]) == 1158310, "third DLC id is 1158310");

    // --- is_dlc_unlocked ---------------------------------------------------
    CHECK(is_dlc_unlocked(394360), "is_dlc_unlocked true for listed id");
    CHECK(is_dlc_unlocked(281990), "is_dlc_unlocked true for second listed id");
    CHECK(!is_dlc_unlocked(123456), "is_dlc_unlocked false for unknown id");
    CHECK(!is_dlc_unlocked(0), "is_dlc_unlocked false for zero id");

    // --- ini sections survive ----------------------------------------------
    CHECK(ini["methods"]["disable_steamapps_issubscribedapp"] == "true",
          "methods section read");
    CHECK(ini["config"]["issubscribedapp_on_false_use_real"] == "true",
          "config section read");
    CHECK(ini["dlc"].has("394360"), "dlc section contains key 394360");

    // --- resolve_config_path ----------------------------------------------
    CHECK(resolve_config_path(kTestIni) == kTestIni,
          "file path stays unchanged");
    CHECK(resolve_config_path("/tmp") == "/tmp/cream_api.ini",
          "directory path gets /cream_api.ini appended");
    CHECK(resolve_config_path("/nonexistent/path.ini") == "/nonexistent/path.ini",
          "nonexistent path stays unchanged");

    // --- empty config is tolerated ----------------------------------------
    dlcs.clear();
    ini.clear();
    const char* kEmptyIni = "/tmp/creamlinux_test_empty.ini";
    FILE* f = fopen(kEmptyIni, "w");
    fclose(f);
    load_config(kEmptyIni);
    CHECK(dlcs.empty(), "empty ini yields empty DLC list");

    if (failures == 0) {
        printf("\nAll tests passed.\n");
        return 0;
    }
    printf("\n%d test(s) FAILED.\n", failures);
    return 1;
}
