#!/bin/bash
# Build the 64-bit VR spectator viewer with MSYS2 MINGW64.
# Usage: vr/viewer/build.sh
# Output: build/vr-viewer/syndwars-vr.exe (with the DLLs it needs)
if [ -z "${VR_IN_MSYS:-}" ] && [ -x /c/msys64/usr/bin/bash ]; then
    DIR="$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)"
    exec env VR_IN_MSYS=1 MSYSTEM=MINGW64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
        "cd \"\$(cygpath -u '$DIR')\" && ./build.sh"
fi
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
OUT="$REPO/build/vr-viewer"
mkdir -p "$OUT"
g++ -std=c++17 -O2 -g -Wall -Wno-unused-function -mconsole \
    -I"$REPO/src" -o "$OUT/syndwars-vr.exe" "$HERE/main.cpp" \
    -lmingw32 -lSDL2main -lSDL2 -lglew32 -lopengl32 -lopenxr_loader
# Bundle runtime DLLs
for dll in $(ldd "$OUT/syndwars-vr.exe" | awk '/mingw64/ {print $3}'); do
    cp -u "$dll" "$OUT/"
done
echo "BUILD OK: $OUT/syndwars-vr.exe"
