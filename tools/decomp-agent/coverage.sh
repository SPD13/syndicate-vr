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
SCENARIOS="${*:-0,1 0,5 0,20 1,2 1,40 2,1}"
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
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    SWARS_DIFFTEST_FN=none SWARS_DIFFTEST_TURNS="$TURNS" SWARS_DIFFTEST_FAST=1 \
    SWARS_DIFFTEST_MIN=0 SWARS_DIFFTEST_LOG="$OUT/difftest.log" \
    SWARS_COVERAGE="$OUT/cov_${SC/,/_}.txt" \
        timeout 900 "$EXE" -q -W -m "$SC" > /dev/null 2>&1 || echo "scenario $SC: rc=$?"
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
