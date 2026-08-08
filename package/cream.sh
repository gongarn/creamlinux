#!/bin/bash
# cream.sh - launch a game with creamlinux DLC unlock hooks.
#
# Usage in Steam launch options:
#   sh ./cream.sh %command%
#   sh ./cream.sh mangohud %command%   # with MangoHud (see below)
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

copy_file "$PWD/lib32Creamlinux.so" /tmp/lib32Creamlinux.so
copy_file "$PWD/lib64Creamlinux.so" /tmp/lib64Creamlinux.so
copy_file "$LIBSTEAM_API_DIR/libsteam_api.so" /tmp/libsteam_api.so

LD_PRELOAD="$LD_PRELOAD /tmp/lib64Creamlinux.so /tmp/lib32Creamlinux.so /tmp/libsteam_api.so" "$@"
EXITCODE=$?
rm -f /tmp/lib32Creamlinux.so /tmp/lib64Creamlinux.so /tmp/libsteam_api.so
exit $EXITCODE
