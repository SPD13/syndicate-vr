#!/bin/sh
# Run a command inside the MSYS2 MINGW32 environment from any shell
# (Git Bash or MSYS2 itself), in the current directory.
# Usage: tools/decomp-agent/msys.sh '<shell command>'
MSYS_ROOT="${MSYS_ROOT:-/c/msys64}"
if [ "$MSYSTEM" = "MINGW32" ]; then
    exec sh -c "$1"
fi
WINCWD="$(pwd -W 2>/dev/null || cygpath -m "$PWD")"
exec env MSYSTEM=MINGW32 CHERE_INVOKING=1 "$MSYS_ROOT/usr/bin/bash" -lc "cd \"\$(cygpath -u '$WINCWD')\" && $1"
