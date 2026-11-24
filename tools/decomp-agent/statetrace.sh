#!/bin/bash
# Whole-game state trace of the mission scenarios, for comparing builds.
# Usage: tools/decomp-agent/statetrace.sh <outdir> [scenario ...]
# Runs build/difftest (or $SWARS_TRACE_BUILD) of this worktree with no function shadowed and writes
# <outdir>/<scenario>.trace: one line per turn with a hash of the things
# memory and the random seed. Two builds behave the same on these scenarios
# when `diff -r` of their outdirs is empty. Used to check routing changes,
# which the per-function difftest cannot see (both sides use the same
# wrapper). Game data from $SWARS_GAMEDATA as in difftest_run.sh.
if [ "${MSYSTEM:-}" != "MINGW32" ] && [ -x /c/msys64/usr/bin/bash ]; then
    exec "$(dirname "$0")/msys.sh" "$(printf "%q " "$0" "$@")"
fi
set -u
OUTDIR="$(mkdir -p "$1" && cd "$1" && pwd)"; shift
SCENARIOS="${*:-0,1 0,5 0,20 1,2 1,40 2,1 0,21 1,27 1,45 0,23 1,16 2,3 1,19 1,2+bot 1,16+bot 1,40+bot 0,23+bot 0,21+bot 0,20+bot 0,5+bot 1,19+bot 1,102+bot 0,22+bot 1,100+bot 1,85+bot+wreck 1,71+bot 1,72+bot+wreck 0,49+bot 3,4+bot 0,92+bot 3,91+bot 3,38+bot 3,61+bot 1,51+bot 0,101+bot 0,1+bot 0,8+bot 0,1+tri 2,1+tri 1,2+tri 1,2+kbd+bot 0,5+kbd+bot 1,2+crater+bot 0,23+crater+bot 2,1+bot 0,23+t_train+t_traffic 3,61+t_ship+t_fly 1,2+m_fissure+m_cryo+m_typing 0,1+bot+m_bat 1,2+bot+p_people+p_net 0,101+bot+p_mech 0,5+bot+m_pause}"
TURNS="${SWARS_DIFFTEST_TURNS:-1500}"
# No window and no sound device: with a real window the mouse pointer of
# the desktop can lie over the game, and the game picks the thing under it
# while drawing, which changes the run
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}" SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMMON="$(git -C "$REPO" rev-parse --path-format=absolute --git-common-dir)"
GAMEDATA="${SWARS_GAMEDATA:-$(dirname "$(dirname "$COMMON")")/gamedata}"
# SWARS_TRACE_BUILD: build/ folder to run, default difftest; harness and
# x64 (build.sh --harness/--x64) have the trace without the asm
BUILD="${SWARS_TRACE_BUILD:-difftest}"
# SWARS_TRACE_EXE=swcore_headless runs the test front end of the game
# library instead; its output (turn and state hash read through swcore.h)
# goes to <outdir>/<scenario>.out; SWARS_TRACE_EXE_ARGS are its own options
# (e.g. --no-draw). The game runs with --original-bugs, to compare with the
# original; swcore_headless "--fixes 1" switches the bug fixes back on.
PROG="${SWARS_TRACE_EXE:-syndwarsfx}"
EXE="$REPO/build/$BUILD/src/$PROG.exe"
[ -f "$EXE" ] || EXE="$REPO/build/$BUILD/src/$PROG"
if [ ! -f "$EXE" ]; then echo "ERROR: no $BUILD build at $EXE"; exit 1; fi
for d in "$GAMEDATA"/*.dll; do
    [ -f "$d" ] && [ ! -f "$(dirname "$EXE")/$(basename "$d")" ] && cp "$d" "$(dirname "$EXE")/"
done

cd "$GAMEDATA"
RCALL=0
for SC in $SCENARIOS; do
    # "<campaign>,<mission>+bot": the mission with scripted player input;
    # "+tri": the map triangulations are rebuilt at the start (pathtrig asm);
    # "+kbd": keyboard control mode; "+crater": a crater every 64 turns;
    # "+wreck": a vehicle or mounted gun is hit hard every 32 turns;
    # the whole suffix goes to SWARS_DIFFTEST_SUFFIX for the hooks in
    # src/difftest_hook_*.c (difftest_suffix_has())
    BOT=""; case "$SC" in *+bot*) BOT=1 ;; esac
    TRI=""; case "$SC" in *+tri*) TRI=1 ;; esac
    KBD=""; case "$SC" in *+kbd*) KBD=1 ;; esac
    CRA=""; case "$SC" in *+crater*) CRA=1 ;; esac
    WRK=""; case "$SC" in *+wreck*) WRK=1 ;; esac
    SUF=""; case "$SC" in *+*) SUF="+${SC#*+}" ;; esac
    T="$OUTDIR/${SC/,/_}.trace"
    OUTF=/dev/null; [ -n "${SWARS_TRACE_EXE:-}" ] && OUTF="$OUTDIR/${SC/,/_}.out"
    # A function name that matches nothing enables the turn limit only
    SWARS_DIFFTEST_FN=statetrace_none SWARS_DIFFTEST_TURNS="$TURNS" \
    SWARS_DIFFTEST_FAST=1 SWARS_DIFFTEST_LOG="$OUTDIR/${SC/,/_}.log" \
    SWARS_STATETRACE="$T" SWARS_SEED="${SWARS_SEED:-12345}" timeout 600 env ${BOT:+SWARS_DIFFTEST_BOT=1} ${TRI:+SWARS_DIFFTEST_RETRI=1} ${KBD:+SWARS_DIFFTEST_KBD=1} ${CRA:+SWARS_DIFFTEST_CRATER=1} ${WRK:+SWARS_DIFFTEST_WRECK=1} ${SUF:+SWARS_DIFFTEST_SUFFIX=$SUF} "$EXE" ${SWARS_TRACE_EXE_ARGS:-} --original-bugs -q -W -m "${SC%%+*}" > "$OUTF" 2>&1
    RC=$?
    echo "  scenario $SC: rc=$RC turns=$(wc -l < "$T" 2>/dev/null || echo 0)"
    # 4: no calls of the (absent) function, the normal end
    [ $RC -eq 0 ] || [ $RC -eq 4 ] || RCALL=1
done
exit $RCALL
