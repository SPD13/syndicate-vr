#!/bin/bash
# Build the 64-bit VR viewer with MSYS2 MINGW64.
# Usage: vr/viewer/build.sh
# Output, in build/vr-viewer (with the DLLs they need), from the 64-bit
# build (build/x64, made by tools/decomp-agent/build.sh --x64):
# - syndwars-vr.exe: the headset with the desktop game in the desktop
#   window (VR_DESKTOP_GAME): the whole game as syndwarsfx (core, desktop
#   front end, renderer, test hooks) and the world export (libswarsview);
# - syndwars-vr-table.exe: the game core alone (libswarscore, the null
#   front end libswarsnull, libswarsview, the engine's world data, not the
#   renderer), with the 3D table in the desktop window; syndwars-vr.exe
#   runs it for --desktop, --companion table, or without a headset.
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
for lib in src/libswarscore.a src/libswarsview.a src/libswarsnull.a src/libswarsfe.a src/libswarstest.a \
        lib/libswworld.a lib/libswrender.a lib/libbullfrog.a lib/libbfsound.a; do
    if [ ! -f "$GAME/$lib" ]; then
        echo "missing $GAME/$lib: run tools/decomp-agent/build.sh --x64 first"
        exit 1
    fi
done
mkdir -p "$OUT"
# the single-header libraries change never: compile them once
if [ ! -f "$OUT/third_party.o" ] || [ "$HERE/third_party.cpp" -nt "$OUT/third_party.o" ]; then
    g++ -std=c++17 -O2 -c -o "$OUT/third_party.o" "$HERE/third_party.cpp"
fi
SYSLIBS="-lmingw32 -lSDL2main -lSDL2 -lglew32 -lopengl32 -lopenxr_loader -lopenal -lvorbisfile -lpng16"
# the table build: the game core alone
g++ -std=c++17 -O2 -g -Wall -Wno-unused-function -mconsole -DVR_DESKTOP_GAME=0 \
    -I"$REPO/src" -o "$OUT/syndwars-vr-table.exe" "$HERE/main.cpp" "$OUT/third_party.o" \
    -Wl,--start-group "$GAME/src/libswarsview.a" "$GAME/src/libswarscore.a" "$GAME/src/libswarsnull.a" \
    "$GAME/lib/libswworld.a" "$GAME/lib/libbullfrog.a" "$GAME/lib/libbfsound.a" -Wl,--end-group \
    $SYSLIBS
# the game build: the whole libraries, as syndwarsfx links them
g++ -std=c++17 -O2 -g -Wall -Wno-unused-function -mconsole -DVR_DESKTOP_GAME=1 \
    -I"$REPO/src" -o "$OUT/syndwars-vr.exe" "$HERE/main.cpp" "$OUT/third_party.o" \
    -Wl,--whole-archive "$GAME/src/libswarscore.a" "$GAME/src/libswarsfe.a" "$GAME/src/libswarstest.a" \
    "$GAME/src/libswarsview.a" -Wl,--no-whole-archive \
    -Wl,--start-group "$GAME/lib/libswrender.a" "$GAME/lib/libswworld.a" "$GAME/lib/libbullfrog.a" \
    "$GAME/lib/libbfsound.a" -Wl,--end-group \
    $SYSLIBS
# the upgraded look's font (text.h), next to the program
mkdir -p "$OUT/fonts"
cp -u "$HERE/fonts/ShareTechMono-Regular.ttf" "$HERE/fonts/OFL.txt" "$OUT/fonts/"
# Bundle runtime DLLs
for dll in $(ldd "$OUT/syndwars-vr.exe" | awk '/mingw64/ {print $3}'); do
    cp -u "$dll" "$OUT/"
done
echo "BUILD OK: $OUT/syndwars-vr.exe, $OUT/syndwars-vr-table.exe"
