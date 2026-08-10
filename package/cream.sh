#!/bin/bash
# cream.sh - launch a game with creamlinux DLC unlock hooks.
#
# Usage in Steam launch options:
#   sh ./cream.sh %command%
#   sh ./cream.sh mangohud %command%   # with MangoHud (see below)
#
# Notes:
# - Steam may launch games from a different working directory (issue #62),
#   so all paths are resolved relative to this script's own location.
# - The libraries are copied into a private temp dir: unique per launch
#   (no races between parallel game starts) and guaranteed to contain no
#   spaces, which LD_PRELOAD cannot handle. Steam Linux Runtime
#   (pressure-vessel) shares the host /tmp with the game container, so the
#   preload also works for games launched through a runtime (issue #71).
copy_file() {
    cp "$1" "$2" || { echo "Error: Failed to copy $1 to $2"; exit 1; }
}

# Steam may launch games from a different working directory (issue #62), so
# always resolve paths relative to this script's own location.
cd "$(dirname "$(readlink -f "$0")")"

# MangoHud compatibility (issue #51): MangoHud's wrapper appends itself to
# LD_PRELOAD, and our dlsym interposition must not confuse it. MANGOHUD_DLSYM
# is a no-op on modern MangoHud (dlsym is the default) but keeps older
# versions working.
if [ "$1" = "mangohud" ]; then
    export MANGOHUD=1
    export MANGOHUD_DLSYM=1
    echo "cream.sh: MangoHud detected, setting MANGOHUD=1 and MANGOHUD_DLSYM=1"
fi

LIBSTEAM_API_DIR=$(find . -name "libsteam_api.so" -printf "%h\n" | head -n 1)
[ -z "$LIBSTEAM_API_DIR" ] && { echo "Error: libsteam_api.so not found."; exit 1; }
if [ ! -z "$CREAM_CONFIG_PATH" ]; then
    # Accept both a path to the ini file and a directory containing cream_api.ini
    if [ -d "$CREAM_CONFIG_PATH" ]; then
        CREAM_CONFIG_PATH="$CREAM_CONFIG_PATH/cream_api.ini"
    fi
    if [ ! -f "$CREAM_CONFIG_PATH" ]; then
        echo "Error: cream_api.ini not found at CREAM_CONFIG_PATH ($CREAM_CONFIG_PATH)."; exit 1;
    fi
    export CREAM_CONFIG_PATH
else
    if [ ! -f "$PWD/cream_api.ini" ]; then
        echo "Error: cream_api.ini not found in the current working directory."; exit 1;
    fi
fi
if [ -z "$CREAM_CONFIG_PATH" ] && [ "$LIBSTEAM_API_DIR" != "$PWD" ]; then
    export CREAM_CONFIG_PATH="$PWD/cream_api.ini"
fi

# Private temp dir; if TMPDIR itself contains spaces (LD_PRELOAD cannot
# handle them), fall back to /tmp.
if [ -n "${TMPDIR:-}" ] && printf '%s' "$TMPDIR" | grep -q ' '; then
    TMPDIR=/tmp
fi
CREAM_TMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/creamlinux.XXXXXX") || {
    echo "Error: cannot create temporary directory."; exit 1;
}
trap 'rm -rf "$CREAM_TMPDIR"' EXIT

copy_file "$PWD/lib32Creamlinux.so" "$CREAM_TMPDIR/lib32Creamlinux.so"
copy_file "$PWD/lib64Creamlinux.so" "$CREAM_TMPDIR/lib64Creamlinux.so"
copy_file "$LIBSTEAM_API_DIR/libsteam_api.so" "$CREAM_TMPDIR/libsteam_api.so"

# Preload only the creamlinux library matching the game's bitness; ld.so
# would otherwise log 'wrong ELF class' noise for the other one (issue #71).
# ELF class byte at offset 4: 01 = 32-bit (ELFCLASS32), 02 = 64-bit (ELFCLASS64).
ELF_CLASS=$(od -An -t x1 -j 4 -N 1 "$CREAM_TMPDIR/libsteam_api.so" 2>/dev/null | tr -d ' \n')
case "$ELF_CLASS" in
    01) PRELOAD_LIBS="$CREAM_TMPDIR/lib32Creamlinux.so" ;;
    02) PRELOAD_LIBS="$CREAM_TMPDIR/lib64Creamlinux.so" ;;
    *)  PRELOAD_LIBS="$CREAM_TMPDIR/lib64Creamlinux.so $CREAM_TMPDIR/lib32Creamlinux.so" ;; # unknown: both
esac

LD_PRELOAD="$LD_PRELOAD $PRELOAD_LIBS $CREAM_TMPDIR/libsteam_api.so" "$@"
EXITCODE=$?
rm -rf "$CREAM_TMPDIR"
exit $EXITCODE
