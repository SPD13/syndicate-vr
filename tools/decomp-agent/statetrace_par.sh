#!/bin/bash
# Linux: runs the state trace scenarios of statetrace.sh split over N jobs,
# each with its own game data copy (hard links) and config dir.
# Usage: tools/decomp-agent/statetrace_par.sh <build> <outdir> [jobs=8]
#   <build> is a build/ folder (x64, arm64, ...). Game data from
#   $SWARS_GAMEDATA (default ~/gamedata); copies go to ~/gd_par/<build>-<j>.
#   For arm64 the qemu and sysroot variables are set here.
# Compare with a reference: `diff -r` (Windows traces before 2026-11-23
# had CRLF line ends; strip them with `tr -d '\r'`).
BUILD="$1"; OUT="$2"; JOBS="${3:-8}"
[ -n "$OUT" ] || { echo "usage: $0 <build> <outdir> [jobs]"; exit 2; }
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
GAMEDATA="${SWARS_GAMEDATA:-$HOME/gamedata}"
ALL=$(sed -n 's/^SCENARIOS="\${\*:-\(.*\)}"$/\1/p' "$REPO/tools/decomp-agent/statetrace.sh")
[ -n "$ALL" ] || { echo "no scenario list in statetrace.sh"; exit 1; }
mkdir -p "$OUT"
i=0
declare -a PART
for sc in $ALL; do PART[$((i % JOBS))]+="$sc "; i=$((i+1)); done
SYSROOT="${SWARS_ARM64_SYSROOT:-$HOME/arm64-sysroot}"
export LD_LIBRARY_PATH="$SYSROOT/usr/lib/aarch64-linux-gnu:$SYSROOT/usr/lib/aarch64-linux-gnu/pulseaudio${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QEMU_LD_PREFIX="${QEMU_LD_PREFIX:-/usr/aarch64-linux-gnu}"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
[ "$BUILD" = arm64 ] || unset LD_LIBRARY_PATH QEMU_LD_PREFIX
for j in $(seq 0 $((JOBS-1))); do
    [ -n "${PART[$j]}" ] || continue
    G="$HOME/gd_par/$BUILD-$j"
    [ -d "$G" ] || { mkdir -p "$HOME/gd_par"; cp -al "$GAMEDATA" "$G"; }
    rm -f "$G/error.log"
    mkdir -p "$HOME/cfg_par/$BUILD-$j"
    ( XDG_CONFIG_HOME="$HOME/cfg_par/$BUILD-$j" SWARS_GAMEDATA="$G" SWARS_TRACE_BUILD="$BUILD" \
        "$REPO/tools/decomp-agent/statetrace.sh" "$OUT" ${PART[$j]} > "$OUT/job$j.out" 2>&1 ) &
done
wait
cat "$OUT"/job*.out | sort
! grep -h "scenario" "$OUT"/job*.out | grep -qv "rc=[04] "
