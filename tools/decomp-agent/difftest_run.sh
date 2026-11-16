#!/bin/bash
# Differential test of one remade function: asm reference vs C.
# Usage: tools/decomp-agent/difftest_run.sh <function> [scenario ...]
#   scenario is "<campaign>,<mission>" as accepted by the game -m option.
# Needs a build from `build.sh --difftest`. Game data path comes from
# $SWARS_GAMEDATA, default is "gamedata" next to the main repo.
# Prints one verdict line: PASS, MISMATCH, UNVERIFIED or ERROR; exit code
# 0 for PASS, 3 MISMATCH, 4 UNVERIFIED, 1 ERROR.
# On Windows, re-run inside MSYS2 MINGW32 when called from another shell
if [ "${MSYSTEM:-}" != "MINGW32" ] && [ -x /c/msys64/usr/bin/bash ]; then
    exec "$(dirname "$0")/msys.sh" "$(printf "%q " "$0" "$@")"
fi
set -u
FN="$1"; shift
SCENARIOS="${*:-0,1 0,5 0,20 1,2 1,40 2,1 0,21 1,27 1,45 0,23 1,16 2,3 1,19 1,2+bot 1,16+bot 1,40+bot 0,23+bot 0,21+bot 0,20+bot 0,5+bot 1,19+bot 1,102+bot 0,22+bot 1,100+bot 1,85+bot 1,71+bot 1,72+bot 0,49+bot 3,4+bot 0,92+bot 3,91+bot 3,38+bot 3,61+bot 1,51+bot 0,101+bot 0,1+bot 0,8+bot 0,1+tri 2,1+tri 1,2+tri 1,2+kbd+bot 0,5+kbd+bot 1,2+crater+bot 0,23+crater+bot}"
TURNS="${SWARS_DIFFTEST_TURNS:-1500}"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMMON="$(git -C "$REPO" rev-parse --path-format=absolute --git-common-dir)"
GAMEDATA="${SWARS_GAMEDATA:-$(dirname "$(dirname "$COMMON")")/gamedata}"
EXE="$REPO/build/difftest/src/syndwarsfx.exe"
[ -f "$EXE" ] || EXE="$REPO/build/difftest/src/syndwarsfx"
OUT="$REPO/build/difftest/runs/$FN"
mkdir -p "$OUT"; rm -f "$OUT"/*

if [ ! -f "$EXE" ]; then echo "ERROR: no difftest build at $EXE"; exit 1; fi
if [ ! -d "$GAMEDATA/data" ]; then echo "ERROR: no game data at $GAMEDATA"; exit 1; fi
# Windows: make the exe find the same DLLs the installed game uses
for d in "$GAMEDATA"/*.dll; do
    [ -f "$d" ] && [ ! -f "$(dirname "$EXE")/$(basename "$d")" ] && cp "$d" "$(dirname "$EXE")/"
done

# Symbol table for resolving DIFF addresses
NM_FILE="$OUT/symbols.txt"
nm -n "$EXE" 2>/dev/null | grep -E ' [bBdDrR] |__image_base__' > "$NM_FILE"

symbolize() {
    # Appends "sym+off" to DIFF lines whose address is within static data
    python -I - "$NM_FILE" "$1" <<'PYEOF'
import sys, bisect, re
syms = []
base = 0
for l in open(sys.argv[1]):
    p = l.split()
    if len(p) == 3:
        if p[2] == '__image_base__':
            base = int(p[0], 16)
        else:
            syms.append((int(p[0], 16), p[2]))
addrs = [a for a, _ in syms]
for l in open(sys.argv[2]):
    m = re.search(r'DIFF region=(\S+) addr=0x[0-9a-f]+ rva=0x([0-9a-f]+)', l)
    if m and m.group(1) not in ('arena0', 'arena1', 'stack') and addrs:
        a = base + int(m.group(2), 16)
        i = bisect.bisect_right(addrs, a) - 1
        if i >= 0:
            l = l.rstrip('\n') + ' sym=%s+0x%x\n' % (syms[i][1], a - syms[i][0])
    sys.stdout.write(l)
PYEOF
}

TOTAL_CMP=0
VERDICT=PASS
cd "$GAMEDATA"

# Random-input cases, if the function has one
LOG="$OUT/fuzz.log"
SWARS_DIFFTEST_FN="$FN" SWARS_DIFFTEST_LOG="$LOG" timeout 300 "$EXE" -t > "$OUT/fuzz.out" 2>&1
RC=$?
case $RC in
0) N=$(grep -o 'compared=[0-9]*' "$LOG" | tail -1 | cut -d= -f2); TOTAL_CMP=$((TOTAL_CMP + ${N:-0}))
   echo "  fuzz: PASS compared=${N:-0}" ;;
4) echo "  fuzz: no case" ;;
3) echo "  fuzz: MISMATCH"; symbolize "$LOG" | grep -v '^difftest: start' | head -40; VERDICT=MISMATCH ;;
*) echo "  fuzz: CRASH rc=$RC"; VERDICT=ERROR ;;
esac

for SC in $SCENARIOS; do
    # "<campaign>,<mission>+bot": the mission with scripted player input;
    # "+tri": the map triangulations are rebuilt at the start (pathtrig asm);
    # "+kbd": keyboard control mode; "+crater": a crater every 64 turns
    BOT=""; case "$SC" in *+bot*) BOT=1 ;; esac
    TRI=""; case "$SC" in *+tri*) TRI=1 ;; esac
    KBD=""; case "$SC" in *+kbd*) KBD=1 ;; esac
    CRA=""; case "$SC" in *+crater*) CRA=1 ;; esac
    [ "$VERDICT" = PASS ] || break
    LOG="$OUT/scenario_${SC/,/_}.log"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    SWARS_DIFFTEST_FN="$FN" SWARS_SEED="${SWARS_SEED:-12345}" SWARS_DIFFTEST_TURNS="$TURNS" SWARS_DIFFTEST_FAST=1 \
    SWARS_DIFFTEST_MIN=0 SWARS_DIFFTEST_LOG="$LOG" \
        timeout 600 env ${BOT:+SWARS_DIFFTEST_BOT=1} ${TRI:+SWARS_DIFFTEST_RETRI=1} ${KBD:+SWARS_DIFFTEST_KBD=1} ${CRA:+SWARS_DIFFTEST_CRATER=1} "$EXE" -q -W -m "${SC%%+*}" > "$OUT/scenario_${SC/,/_}.out" 2>&1
    RC=$?
    cp -f error.log "$OUT/scenario_${SC/,/_}.error.log" 2>/dev/null
    case $RC in
    0) N=$(grep -o 'compared=[0-9]*' "$LOG" | tail -1 | cut -d= -f2)
       C=$(grep -o 'calls=[0-9]*' "$LOG" | tail -1 | cut -d= -f2)
       TOTAL_CMP=$((TOTAL_CMP + ${N:-0}))
       echo "  scenario $SC: PASS calls=${C:-0} compared=${N:-0}" ;;
    3) echo "  scenario $SC: MISMATCH"; symbolize "$LOG" | grep -v '^\s*region\|^difftest: start' | head -60
       VERDICT=MISMATCH ;;
    124) echo "  scenario $SC: TIMEOUT"; VERDICT=ERROR ;;
    *) echo "  scenario $SC: CRASH rc=$RC"; tail -5 "$OUT/scenario_${SC/,/_}.error.log"; VERDICT=ERROR ;;
    esac
done

if [ "$VERDICT" = PASS ] && [ "$TOTAL_CMP" -eq 0 ]; then
    VERDICT=UNVERIFIED
fi
echo "$VERDICT fn=$FN compared=$TOTAL_CMP logs=$OUT"
case $VERDICT in
PASS) exit 0 ;; MISMATCH) exit 3 ;; UNVERIFIED) exit 4 ;; *) exit 1 ;;
esac
