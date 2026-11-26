#!/bin/bash
# Checks that the web build of the game core (tools/web/build_core.sh) plays
# the same game as a native one: runs the core-only scenarios of
# coreturn_compare.py --core-only --view (the 52 of statetrace.sh without
# hooks of front-end code) under node, with the same options, and compares
# the state hash of every turn with the core run of a native
# `coreturn_compare.py harness --core-only --view` (its build/coreturn/core).
# Usage (Linux with emsdk; game data folder in $SWARS_GAMEDATA):
#   tools/web/check_core.sh <native build/coreturn/core folder> [outdir]
set -e
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
REF="$1"; OUT="${2:-$REPO/build/wasm/traces}"
[ -d "$REF" ] || { echo "usage: $0 <native build/coreturn/core> [outdir]"; exit 2; }
[ -n "${SWARS_GAMEDATA:-}" ] || { echo "set SWARS_GAMEDATA to the game data folder"; exit 2; }
# shellcheck disable=SC1091
source "${EMSDK:-$HOME/emsdk}/emsdk_env.sh" > /dev/null 2>&1
cd "$REPO"
ALL=$(sed -n 's/^SCENARIOS="\${\*:-\(.*\)}"$/\1/p' tools/decomp-agent/statetrace.sh)
SEL=""
for sc in $ALL; do
    case "$sc" in *m_cryo*|*m_typing*|*m_pause*) ;; *) SEL="$SEL $sc" ;; esac
done
rm -rf "$OUT"; mkdir -p "$OUT"
SWARS_TRACE_BUILD=wasm SWARS_TRACE_EXE=swcore_coreonly SWARS_TRACE_RUNNER=node \
  SWARS_TRACE_EXE_ARGS="--core-turn --view --status --fixes 1 --turns 1500" \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  tools/decomp-agent/statetrace.sh "$OUT" $SEL > "$OUT/run.log" 2>&1 || true
hashes() { tr -d '\r' < "$1" | grep -E '^[0-9]+ [0-9a-f]{8}$'; }
n=0; bad=0
for sc in $SEL; do
    f="${sc/,/_}.out"
    n=$((n + 1))
    if [ ! -s "$OUT/$f" ] || [ ! -s "$REF/$f" ] || [ "$(hashes "$OUT/$f" | md5sum)" != "$(hashes "$REF/$f" | md5sum)" ]; then
        bad=$((bad + 1))
        echo "$sc differs"
    fi
done
echo "wasm: $n scenarios, $bad differ"
[ $bad -eq 0 ]
