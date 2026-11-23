#!/bin/bash
# Runs a build of syndwarsfx on a mission for a few seconds, from the game
# data folder, and prints the end of its output and log.
# Usage: tools/decomp-agent/run_x64.sh [build=x64] [mission=1,2] [seconds=20]
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-x64}"; MISSION="${2:-1,2}"; SECS="${3:-20}"
COMMON="$(git -C "$REPO" rev-parse --path-format=absolute --git-common-dir)"
GAMEDATA="${SWARS_GAMEDATA:-$(dirname "$(dirname "$COMMON")")/gamedata}"
EXE="$REPO/build/$BUILD/src/syndwarsfx.exe"
cd "$GAMEDATA" || exit 1
rm -f error.log
timeout "$SECS" "$EXE" -q -W -m "$MISSION" > "$REPO/build/run_$BUILD.out" 2>&1
RC=$?
echo "rc=$RC (124 = still running at timeout)"
tail -5 "$REPO/build/run_$BUILD.out"
[ -f error.log ] && { echo "--- error.log"; grep -iE "error|warn|assert|fail" error.log | tail -10; }
exit 0
