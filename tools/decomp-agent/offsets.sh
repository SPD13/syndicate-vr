#!/bin/bash
# Print sizeof and field offsets of a game struct, to map asm offsets
# like `0xc(%edx,%eax,4)` to field names.
# Usage: tools/decomp-agent/offsets.sh <header.h> <StructName> [field ...]
#   e.g. offsets.sh thing.h SimpleThing Parent LinkChild Flag U.ULight.MinBright
# Without fields, prints offsets of all top-level fields found in the header.
# On Windows, re-run inside MSYS2 MINGW32 when called from another shell
if [ "${MSYSTEM:-}" != "MINGW32" ] && [ -x /c/msys64/usr/bin/bash ]; then
    exec "$(dirname "$0")/msys.sh" "$(printf "%q " "$0" "$@")"
fi
set -e
HDR="$1"; STRUCT="$2"; shift 2
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BDIR="$REPO/build/release"
[ -d "$BDIR/src" ] || BDIR="$REPO/build/difftest"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

FIELDS="$*"
if [ -z "$FIELDS" ]; then
    # crude extraction of top-level field names from the struct definition
    FIELDS=$(python -I - "$REPO/src/$HDR" "$STRUCT" <<'PYEOF'
import re, sys
src = open(sys.argv[1], errors='replace').read()
src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)
src = re.sub(r'//.*', '', src)
m = re.search(r'struct\s+' + sys.argv[2] + r'\s*\{', src)
if m:
    i = m.end(); depth = 1; buf = ''; fields = []
    while depth > 0 and i < len(src):
        c = src[i]
        if c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 1: buf = ''
        elif c == ';' and depth == 1:
            n = re.findall(r'(\w+)\s*(?:\[[^\]]*\]\s*)*$', buf.strip())
            if n: fields.append(n[0])
            buf = ''
        elif depth == 1: buf += c
        i += 1
    print(' '.join(fields))
PYEOF
)
fi
{
    echo '#include <stdio.h>'
    echo '#include <stddef.h>'
    echo "#include \"$HDR\""
    echo 'int main(void) {'
    echo "    printf(\"sizeof(struct $STRUCT) = %u (0x%x)\\n\", (unsigned)sizeof(struct $STRUCT), (unsigned)sizeof(struct $STRUCT));"
    for f in $FIELDS; do
        echo "    printf(\"  0x%03x %4u  $f (size %u)\\n\", (unsigned)offsetof(struct $STRUCT, $f), (unsigned)offsetof(struct $STRUCT, $f), (unsigned)sizeof(((struct $STRUCT *)0)->$f));"
    done
    echo '    return 0; }'
} > "$TMP/off.c"
gcc -m32 -w -DHAVE_CONFIG_H -I"$BDIR/src" -I"$REPO/src" -I"$BDIR/bflibrary/include" \
    -I"$REPO/bflibrary/include" -I"$REPO/bfsoundlib/include" -I"$REPO/swrendersoft/include" \
    -I"$BDIR/swrendersoft/include" -I"$REPO/bfariadne/include" -I"$REPO/bfsmacklib/include" \
    -I"$REPO/lib/include" -I"$BDIR/lib/include" \
    -o "$TMP/off.exe" "$TMP/off.c" && "$TMP/off.exe"
