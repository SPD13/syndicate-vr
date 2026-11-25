#!/bin/bash
# Links the core objects of a build (those coredeps.py puts in no front-end
# group) with the null front end (src/swfe_null.c) and an empty main, and
# lists what the core still needs from outside: the symbols no object
# defines, and the members of the support libraries the link pulls in.
# Usage: tools/decomp-agent/coreprobe.sh [build] [outdir]
#   build: build/ folder, default harness (32-bit, no asm)
#   outdir: default build/coreprobe
# The SDL and system libraries are left out, so SDL_* symbols show which
# parts of libbullfrog's platform layer the core still reaches.
if [ "${MSYSTEM:-}" != "MINGW32" ] && [ -x /c/msys64/usr/bin/bash ]; then
    exec "$(dirname "$0")/msys.sh" "$(printf "%q " "$0" "$@")"
fi
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-harness}"
OUT="${2:-$REPO/build/coreprobe}"
B="$REPO/build/$BUILD/src"
mkdir -p "$OUT"
cd "$REPO"

MODS=$(python tools/decomp-agent/coredeps.py "$B" | sed -n 's/^core modules ([0-9]*): //p')
OBJS=""
for m in $MODS; do OBJS="$OBJS $B/libswarscore_a-$m.o"; done

# The compile command of the build, from its Makefile (without ccache,
# which needs variables msys.sh does not pass)
COMPILE=$(printf 'print-compile:\n\t@echo $(COMPILE)\n' | make -s -C "$B" -f Makefile -f - print-compile | sed 's/^ccache //')
(cd "$B" && $COMPILE -c "$REPO/src/swfe_null.c" -o "$OUT/swfe_null.o") || exit 1
# (the build defines main as SDL_main)
printf '#undef main\nint main(int argc, char **argv) { return 0; }\n' > "$OUT/main.c"
(cd "$B" && $COMPILE -c "$OUT/main.c" -o "$OUT/main.o") || exit 1

CXX=$(printf 'print-cxx:\n\t@echo $(CXX) $(LDFLAGS)\n' | make -s -C "$B" -f Makefile -f - print-cxx)
$CXX -o "$OUT/coreprobe.exe" "$OUT/main.o" "$OUT/swfe_null.o" $OBJS \
    -L"$REPO/build/$BUILD/lib" -lswworld -lbullfrog -Wl,-Map="$OUT/coreprobe.map" \
    > "$OUT/link.log" 2>&1

grep -o "undefined reference to \`[^']*'" "$OUT/link.log" | sed "s/undefined reference to \`//; s/'$//" \
    | sort -u > "$OUT/undefined.txt"
# Archive members the link pulled, with the object and symbol that pulled each
awk '/^Archive member included/{f=1;next} /^Discarded|^Allocating|^Memory map/{f=0} f' "$OUT/coreprobe.map" \
    | awk 'NF==1 && /\(/ {m=$1; next} NF>=2 && m!="" {print m" <- "$1" "$2; m=""}' \
    | sed -E 's/^[^ ]*\/(lib[a-z]+)\.a\([a-z]+_a-([^)]*)\.o\)/\1:\2/; s/ [^ ]*\/(lib[a-z]+\.a\()?(lib[a-z]+_a-)?([^/ ]*\.o)\)?/ \3/' \
    | grep "^lib\(bullfrog\|swworld\|swrender\)" | sort > "$OUT/members.txt"

echo "core objects: $(echo $MODS | wc -w)"
echo "undefined: $(grep -vc '^SDL_' "$OUT/undefined.txt") game symbols, $(grep -c '^SDL_' "$OUT/undefined.txt") SDL symbols"
grep -v '^SDL_' "$OUT/undefined.txt" | sed 's/^/  /'
echo "library members pulled: $(grep -c "^libswworld" "$OUT/members.txt") of libswworld, $(grep -c "^libswrender" "$OUT/members.txt") of libswrender (none expected), $(grep -c '^libbullfrog' "$OUT/members.txt") of libbullfrog (see $OUT/members.txt)"
