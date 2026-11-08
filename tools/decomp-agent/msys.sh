#!/bin/sh
# Run a command inside the MSYS2 MINGW32 environment from any shell
# (Git Bash or MSYS2 itself), in the current directory.
# Usage: tools/decomp-agent/msys.sh '<shell command>'
# SWARS_* variables are passed on; the MSYS2 login shell does not get the
# environment of a Git Bash caller.
MSYS_ROOT="${MSYS_ROOT:-/c/msys64}"
if [ "$MSYSTEM" = "MINGW32" ]; then
    exec sh -c "$1"
fi
WINCWD="$(pwd -W 2>/dev/null || cygpath -m "$PWD")"
EXPORTS=""
for v in $(env | sed -n 's/^\(SWARS_[A-Za-z0-9_]*\)=.*/\1/p'); do
    eval "val=\$$v"
    case "$val" in
    /[a-zA-Z]/*) val="$(cygpath -m "$val")" ;; # Git Bash path to C:/...
    esac
    EXPORTS="$EXPORTS export $v='$val';"
done
exec env MSYSTEM=MINGW32 CHERE_INVOKING=1 "$MSYS_ROOT/usr/bin/bash" -lc "$EXPORTS cd \"\$(cygpath -u '$WINCWD')\" && $1"
