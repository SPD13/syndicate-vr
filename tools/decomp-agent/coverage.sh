#!/bin/bash
# Measure which C functions run in the difftest scenarios.
# Usage: tools/decomp-agent/coverage.sh [scenario ...]
# Builds build/coverage (-finstrument-functions), runs each scenario and
# writes tools/decomp-agent/coverage.json: {function: calls summed over
# scenarios}. queue.py uses it to prefer targets the difftest can verify.
# On Windows, re-run inside MSYS2 MINGW32 when called from another shell
if [ "${MSYSTEM:-}" != "MINGW32" ] && [ -x /c/msys64/usr/bin/bash ]; then
    exec "$(dirname "$0")/msys.sh" "$(printf "%q " "$0" "$@")"
fi
set -eu
SCENARIOS="${*:-0,1 0,5 0,20 1,2 1,40 2,1 0,21 1,27 1,45 0,23 1,16 2,3 1,19 1,2+bot 1,16+bot 1,40+bot 0,23+bot 0,21+bot 0,20+bot 0,5+bot 1,19+bot 1,102+bot 0,22+bot 1,100+bot 1,85+bot+wreck 1,71+bot 1,72+bot+wreck 0,49+bot 3,4+bot 0,92+bot 3,91+bot 3,38+bot 3,61+bot 1,51+bot 0,101+bot 0,1+bot 0,8+bot 0,1+tri 2,1+tri 1,2+tri 1,2+kbd+bot 0,5+kbd+bot 1,2+crater+bot 0,23+crater+bot 2,1+bot 0,23+t_train+t_traffic 3,61+t_ship+t_fly 1,2+m_fissure+m_cryo+m_typing 0,1+bot+m_bat 1,2+bot+p_people+p_net 0,101+bot+p_mech 0,5+bot+m_pause}"
TURNS="${SWARS_DIFFTEST_TURNS:-1500}"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMMON="$(git -C "$REPO" rev-parse --path-format=absolute --git-common-dir)"
GAMEDATA="${SWARS_GAMEDATA:-$(dirname "$(dirname "$COMMON")")/gamedata}"

"$REPO/tools/decomp-agent/build.sh" --coverage
EXE="$REPO/build/coverage/src/syndwarsfx.exe"
[ -f "$EXE" ] || EXE="$REPO/build/coverage/src/syndwarsfx"
for d in "$GAMEDATA"/*.dll; do
    [ -f "$d" ] && [ ! -f "$(dirname "$EXE")/$(basename "$d")" ] && cp "$d" "$(dirname "$EXE")/"
done
OUT="$REPO/build/coverage/runs"
mkdir -p "$OUT"; rm -f "$OUT"/*
nm -n "$EXE" | grep -E ' [tT] |__image_base__' > "$OUT/symbols.txt"

cd "$GAMEDATA"
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
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    SWARS_DIFFTEST_FN=none SWARS_SEED="${SWARS_SEED:-12345}" SWARS_DIFFTEST_TURNS="$TURNS" SWARS_DIFFTEST_FAST=1 \
    SWARS_DIFFTEST_MIN=0 SWARS_DIFFTEST_LOG="$OUT/difftest.log" \
    SWARS_COVERAGE="$OUT/cov_${SC/,/_}.txt" \
        timeout 900 env ${BOT:+SWARS_DIFFTEST_BOT=1} ${TRI:+SWARS_DIFFTEST_RETRI=1} ${KBD:+SWARS_DIFFTEST_KBD=1} ${CRA:+SWARS_DIFFTEST_CRATER=1} ${WRK:+SWARS_DIFFTEST_WRECK=1} ${SUF:+SWARS_DIFFTEST_SUFFIX=$SUF} "$EXE" --original-bugs -q -W -m "${SC%%+*}" > /dev/null 2>&1 || echo "scenario $SC: rc=$?"
    echo "scenario $SC: $(wc -l < "$OUT/cov_${SC/,/_}.txt" 2>/dev/null || echo 0) functions hit"
done

python -I - "$OUT" "$REPO/tools/decomp-agent/coverage.json" <<'PYEOF'
import glob, json, os, sys
out, dest = sys.argv[1], sys.argv[2]
syms = {}
base = 0
for l in open(os.path.join(out, 'symbols.txt')):
    p = l.split()
    if len(p) == 3:
        if p[2] == '__image_base__':
            base = int(p[0], 16)
        else:
            syms[int(p[0], 16)] = p[2].lstrip('_')
cov = {}
for f in glob.glob(os.path.join(out, 'cov_*.txt')):
    sc = os.path.basename(f)[4:-4].replace('_', ',')
    for l in open(f):
        a, n = l.split()
        # logged as offsets from the image base, as the image is relocated
        name = syms.get(base + int(a, 16))
        if name:
            e = cov.setdefault(name, {'calls': 0, 'scenarios': []})
            e['calls'] += int(n)
            e['scenarios'].append(sc)
json.dump(cov, open(dest, 'w'), indent=0, sort_keys=True)
print('%d functions covered -> %s' % (len(cov), dest))
PYEOF
