#!/usr/bin/env bash
# test_cream_sh.sh - integration test for the cream.sh launch script
# (issues #51, #62, #71):
#   - works from any working directory (#62)
#   - mangohud mode exports MANGOHUD/MANGOHUD_DLSYM (#51)
#   - preloads only the bitness-matching creamlinux lib (#71)
#   - copies the libs into a space-free private temp dir and cleans up
#
# Usage: bash test_cream_sh.sh <package-dir>
set -u

PACKAGE_DIR="${1:-}"
if [ -z "$PACKAGE_DIR" ] || [ ! -f "$PACKAGE_DIR/cream.sh" ]; then
    echo "usage: $0 <package-dir-with-cream.sh>" >&2
    exit 2
fi

failures=0
WORK=$(mktemp -d) || exit 2
trap 'rm -rf "$WORK"' EXIT

# Fake game: a directory WITH SPACES in its name (an LD_PRELOAD hazard),
# libsteam_api.so is a minimal ELF header carrying only the class byte.
GAME="$WORK/My Game With Spaces"
mkdir -p "$GAME/binaries"
printf '\177ELF\002' > "$GAME/binaries/libsteam_api.so"   # ELFCLASS64
printf '[config]\nunlockall = true\n' > "$GAME/cream_api.ini"
cp "$PACKAGE_DIR/cream.sh" "$GAME/cream.sh"
chmod +x "$GAME/cream.sh"
# dummy creamlinux libs: cream.sh only copies them, they are never loaded
printf 'dummy' > "$GAME/lib64Creamlinux.so"
printf 'dummy' > "$GAME/lib32Creamlinux.so"

# run_game <workdir> <outfile> [extra args...] -> runs cream.sh + a shell
# that dumps $LD_PRELOAD (and $MANGOHUD|$MANGOHUD_DLSYM) into <outfile>
run_game() {
    local wd="$1" out="$2"; shift 2
    ( cd "$wd" && sh ./cream.sh "$@" /bin/sh -c \
        'printf "%s\n%s" "$LD_PRELOAD" "$MANGOHUD|$MANGOHUD_DLSYM" > "$1"' \
        _ "$out" ) >/dev/null 2>&1
}

# Fake mangohud wrapper (like /usr/bin/mangohud): cream.sh keeps it as the
# first argument of the launched command, so it must exist in PATH.
FAKEBIN="$WORK/bin"
mkdir -p "$FAKEBIN"
cat > "$FAKEBIN/mangohud" <<'EOF'
#!/bin/sh
exec "$@"
EOF
chmod +x "$FAKEBIN/mangohud"
PATH="$FAKEBIN:$PATH"
export PATH

check() { # check <desc> <command...>
    local desc="$1"; shift
    if "$@"; then
        echo "ok: $desc"
    else
        echo "FAIL: $desc"
        failures=$((failures + 1))
    fi
}

# --- 1. 64-bit game: lib64Creamlinux preloaded, no lib32, no spaces ----
run_game "$GAME" "$WORK/preload64.txt"
check "64-bit: cream.sh exits 0" test -s "$WORK/preload64.txt"
check "64-bit: lib64Creamlinux preloaded" \
    grep -q "lib64Creamlinux.so" "$WORK/preload64.txt"
check "64-bit: lib32Creamlinux NOT preloaded" \
    grep -qv "lib32Creamlinux.so" "$WORK/preload64.txt"
check "64-bit: libsteam_api preloaded" \
    grep -q "libsteam_api.so" "$WORK/preload64.txt"
check "64-bit: preload paths contain no spaces" \
    grep -qv ' ' "$WORK/preload64.txt"
check "64-bit: mangohud vars unset" \
    grep -q "^|$" "$WORK/preload64.txt"

# --- 2. 32-bit game: lib32Creamlinux preloaded instead -----------------
printf '\177ELF\001' > "$GAME/binaries/libsteam_api.so"   # ELFCLASS32
run_game "$GAME" "$WORK/preload32.txt"
check "32-bit: lib32Creamlinux preloaded" \
    grep -q "lib32Creamlinux.so" "$WORK/preload32.txt"
check "32-bit: lib64Creamlinux NOT preloaded" \
    grep -qv "lib64Creamlinux.so" "$WORK/preload32.txt"

# --- 3. mangohud mode ----------------------------------------------------
printf '\177ELF\002' > "$GAME/binaries/libsteam_api.so"
run_game "$GAME" "$WORK/mango.txt" mangohud
check "mangohud: MANGOHUD and MANGOHUD_DLSYM set" \
    grep -q "^1|1$" "$WORK/mango.txt"

# --- 4. CWD independence (#62): launch the script by absolute path ----
# from a different working directory; cream.sh cd's to its own location.
( cd "$WORK" && sh "$GAME/cream.sh" /bin/sh -c \
    'printf "%s\n" "$LD_PRELOAD" > "$1"' _ "$WORK/preload_cwd.txt" \
) >/dev/null 2>&1
check "cwd: works when launched from another directory" \
    grep -q "lib64Creamlinux.so" "$WORK/preload_cwd.txt"

# --- 5. temp dir cleanup --------------------------------------------------
BEFORE=$(ls -d /tmp/creamlinux.* 2>/dev/null | wc -l)
run_game "$GAME" "$WORK/preload_cl.txt"
AFTER=$(ls -d /tmp/creamlinux.* 2>/dev/null | wc -l)
check "cleanup: no leftover /tmp/creamlinux.* dirs" \
    test "$BEFORE" -eq "$AFTER"

# --- 6. missing libsteam_api.so -> clear error ---------------------------
EMPTY="$WORK/Empty Game"
mkdir -p "$EMPTY"
cp "$PACKAGE_DIR/cream.sh" "$EMPTY/cream.sh"
printf 'dummy' > "$EMPTY/lib64Creamlinux.so"
printf 'dummy' > "$EMPTY/lib32Creamlinux.so"
( cd "$EMPTY" && sh ./cream.sh /bin/true ) >"$WORK/err.txt" 2>&1
check "missing libsteam_api.so: nonzero exit" test $? -ne 0
check "missing libsteam_api.so: error message" \
    grep -q "libsteam_api.so not found" "$WORK/err.txt"

if [ "$failures" -eq 0 ]; then
    echo "cream.sh OK"
    exit 0
fi
echo "$failures cream.sh check(s) failed" >&2
exit 1
