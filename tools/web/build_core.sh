#!/bin/bash
# Builds the game core alone (swcore_coreonly: the core, the world export
# libswarsview, the test hooks, the null front end) for the web, with
# Emscripten, from the project's own autotools build; then it runs under
# node with the game data folder (NODERAWFS).
# Run on Linux (WSL) with emsdk installed (default ~/emsdk):
#   tools/web/build_core.sh [--reconf]
# Output: build/wasm/src/swcore_coreonly (JavaScript) + swcore_coreonly.wasm
# Run:    cd <game data> && node <repo>/build/wasm/src/swcore_coreonly \
#           --view --turns 600 -m 1,85
# The libraries Emscripten has as ports (SDL2, libpng, Vorbis) are given
# to configure through pkg-config files in build/wasm/pc; OpenAL is
# Emscripten's own. Nothing of the core uses them: they only satisfy the
# configure checks and the libraries linked with every program.
set -e
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
RECONF=0
[ "${1:-}" = "--reconf" ] && RECONF=1
EMSDK_DIR="${EMSDK:-$HOME/emsdk}"
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" > /dev/null 2>&1
command -v emcc > /dev/null || { echo "emcc not found: install emsdk in $EMSDK_DIR"; exit 1; }
B="$REPO/build/wasm"
mkdir -p "$B/pc"
pc() { # name, flags
    printf 'Name: %s\nDescription: Emscripten port\nVersion: 99\nCflags: %s\nLibs: %s\n' "$1" "$2" "$2" > "$B/pc/$1.pc"
}
pc sdl2 "-sUSE_SDL=2"
pc libpng "-sUSE_LIBPNG=1"
pc vorbisfile "-sUSE_VORBIS=1 -sUSE_OGG=1"
pc vorbis "-sUSE_VORBIS=1 -sUSE_OGG=1"
pc ogg "-sUSE_OGG=1"
# (emconfigure sets its own PKG_CONFIG_LIBDIR: ours goes to configure as
# an argument, which wins)
cd "$REPO"
if [ ! -f configure ]; then
    autoreconf -i > "$B/autoreconf.log" 2>&1 || { tail -20 "$B/autoreconf.log"; exit 1; }
    git checkout -q -- $(git ls-files '*pkg.m4') 2>/dev/null || true
fi
cd "$B"
if [ $RECONF -eq 1 ] || [ ! -f Makefile ]; then
    # -O0 as the native builds; node reads the game data folder directly
    LDF="-sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=1 -sSTACK_SIZE=4MB -sENVIRONMENT=node"
    CFLAGS="-O0 -g" CXXFLAGS="-O0 -g" LDFLAGS="$LDF" \
        emconfigure "$REPO/configure" --host=wasm32-unknown-emscripten --prefix=/ \
        --disable-lb-wscreen-control --disable-sdltest --enable-test-harness --with-data-path=. \
        PKG_CONFIG_LIBDIR="$B/pc" \
        > configure.log 2>&1 || { tail -40 configure.log; exit 1; }
fi
JOBS="${JOBS:-2}"
# the libraries of the subprojects first, as a full make does
: > make.log
for d in bflibrary bfsoundlib swrendersoft; do
    if ! emmake make -C "$d" -j"$JOBS" >> make.log 2>&1; then
        grep -E -B2 -A6 '(error|Error)[: ]' make.log | head -60
        echo "BUILD FAILED in $d (full log: $B/make.log)"; exit 1
    fi
done
if ! emmake make -C src -j"$JOBS" swcore_coreonly >> make.log 2>&1; then
    grep -E -B2 -A6 '(error|Error)[: ]' make.log | head -60
    echo "BUILD FAILED (full log: $B/make.log)"; exit 1
fi
echo "BUILD OK: $B/src/swcore_coreonly"
