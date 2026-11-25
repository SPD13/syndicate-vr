#!/bin/bash
# Build the 64-bit VR viewer with MSYS2 MINGW64.
# Usage: vr/viewer/build.sh
# Output: build/vr-viewer/syndwars-vr.exe (with the DLLs it needs)
# It links the game: the core (libswarscore), the world export
# (libswarsview), the null front end (libswarsnull) and the engine's world
# data and platform libraries, from the 64-bit build (build/x64, made by
# tools/decomp-agent/build.sh --x64); not the software renderer.
if [ -z "${VR_IN_MSYS:-}" ] && [ -x /c/msys64/usr/bin/bash ]; then
    DIR="$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)"
    exec env VR_IN_MSYS=1 MSYSTEM=MINGW64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
        "cd \"\$(cygpath -u '$DIR')\" && ./build.sh"
fi
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
OUT="$REPO/build/vr-viewer"
GAME="$REPO/build/x64"
for lib in src/libswarscore.a src/libswarsview.a src/libswarsnull.a lib/libswworld.a lib/libbullfrog.a lib/libbfsound.a; do
    if [ ! -f "$GAME/$lib" ]; then
        echo "missing $GAME/$lib: run tools/decomp-agent/build.sh --x64 first"
        exit 1
    fi
done
mkdir -p "$OUT"
g++ -std=c++17 -O2 -g -Wall -Wno-unused-function -mconsole \
    -I"$REPO/src" -o "$OUT/syndwars-vr.exe" "$HERE/main.cpp" \
    -Wl,--start-group "$GAME/src/libswarsview.a" "$GAME/src/libswarscore.a" "$GAME/src/libswarsnull.a" \
    "$GAME/lib/libswworld.a" "$GAME/lib/libbullfrog.a" "$GAME/lib/libbfsound.a" -Wl,--end-group \
    -lmingw32 -lSDL2main -lSDL2 -lglew32 -lopengl32 -lopenxr_loader -lopenal -lvorbisfile -lpng16
# Bundle runtime DLLs
for dll in $(ldd "$OUT/syndwars-vr.exe" | awk '/mingw64/ {print $3}'); do
    cp -u "$dll" "$OUT/"
done
echo "BUILD OK: $OUT/syndwars-vr.exe"
