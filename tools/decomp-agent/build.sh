#!/bin/bash
# Build syndwarsfx (32-bit, MSYS2 MINGW32 or Linux -m32; 64-bit with --x64).
# Usage: tools/decomp-agent/build.sh [--check] [--reconf] [--difftest|--coverage|--dos-network|--harness|--x64|--arm64]
#   Builds into <repo>/build/release (or build/difftest with --difftest).
# --harness: 32-bit, with the game-run test harness and no asm (build/harness).
# --x64: 64-bit (MSYS2 MINGW64, or native gcc on Linux), with the test
#   harness, into build/x64; its DLLs are copied next to the exe, so that it
#   does not pick the 32-bit ones of the game data folder.
# --arm64: Linux only, cross build for aarch64 with the test harness into
#   build/arm64, against the libraries of $SWARS_ARM64_SYSROOT (default
#   ~/arm64-sysroot, made of Ubuntu arm64 .debs); runs under qemu-aarch64.
WANT_SYS=MINGW32
for a in "$@"; do [ "$a" = "--x64" ] && WANT_SYS=MINGW64; done
# On Windows, re-run inside MSYS2 when called from another shell (Git Bash
# sets MSYSTEM too; only an MSYS2 shell has pacman)
if { [ "${MSYSTEM:-}" != "$WANT_SYS" ] || ! command -v pacman >/dev/null; } \
        && [ -x /c/msys64/usr/bin/bash ]; then
    SWARS_MSYSTEM=$WANT_SYS exec "$(dirname "$0")/msys.sh" "$(printf "%q " "$0" "$@")"
fi
set -e
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
export CCACHE_BASEDIR="$REPO"
export CCACHE_DIR="${CCACHE_DIR:-$HOME/.ccache}"
export USERPROFILE="${USERPROFILE:-$(cygpath -w "$HOME" 2>/dev/null)}"
CHECK=0; RECONF=0; VARIANT=release; EXTRA_CONF=""; EXTRA_CFLAGS=""
for a in "$@"; do
    case "$a" in
    --check) CHECK=1 ;;
    --reconf) RECONF=1 ;;
    --difftest) VARIANT=difftest; EXTRA_CONF="--enable-difftest" ;;
    --coverage) VARIANT=coverage; EXTRA_CONF="--enable-difftest"; EXTRA_CFLAGS="-finstrument-functions" ;;
    --dos-network) VARIANT=dosnet; EXTRA_CONF="--enable-dos-network" ;;
    --harness) VARIANT=harness; EXTRA_CONF="--enable-test-harness" ;;
    --x64) VARIANT=x64; EXTRA_CONF="--enable-test-harness" ;;
    --arm64) VARIANT=arm64; EXTRA_CONF="--enable-test-harness" ;;
    *) echo "unknown arg $a" >&2; exit 2 ;;
    esac
done
BDIR="$REPO/build/$VARIANT"
JOBS="$(nproc 2>/dev/null || echo 4)"
cd "$REPO"
mkdir -p "$REPO/build"
if [ $RECONF -eq 1 ] || [ ! -f configure ]; then
    autoreconf -i > "$REPO/build/autoreconf.log" 2>&1 || { tail -20 "$REPO/build/autoreconf.log"; exit 1; }
    # autoreconf --install refreshes the vendored pkg.m4 copies; keep the committed ones
    git checkout -q -- $(git ls-files '*pkg.m4') 2>/dev/null || true
fi
mkdir -p "$BDIR"; cd "$BDIR"
if [ $RECONF -eq 1 ] || [ ! -f Makefile ]; then
    FL="-m32 -fno-PIC -fno-PIE"
    [ "$VARIANT" = x64 ] && FL=""
    CC="ccache gcc"; command -v ccache >/dev/null || CC=gcc
    if [ "$VARIANT" = arm64 ]; then
        SYSROOT="${SWARS_ARM64_SYSROOT:-$HOME/arm64-sysroot}"
        ML=aarch64-linux-gnu
        FL="-I$SYSROOT/usr/include -I$SYSROOT/usr/include/$ML"
        LDX="-L$SYSROOT/usr/lib/$ML -Wl,-rpath-link,$SYSROOT/usr/lib/$ML:$SYSROOT/usr/lib/$ML/pulseaudio"
        CC="$ML-gcc"; command -v ccache >/dev/null && CC="ccache $CC"
        export CXX="${SWARS_ARM64_CXX:-$ML-g++}"
        export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/$ML/pkgconfig:$SYSROOT/usr/share/pkgconfig"
        export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
        EXTRA_CONF="$EXTRA_CONF --host=$ML"
    fi
    # Outside Windows the data path is fixed at configure time; the test
    # scripts run the game from the game data folder, as on Windows
    case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) ;; *)
        EXTRA_CONF="$EXTRA_CONF --with-data-path=${SWARS_DATA_PATH:-.}" ;; esac
    CC="$CC" CFLAGS="$FL -g" CXXFLAGS="$FL -g" LDFLAGS="$FL ${LDX:-}" \
        "$REPO/configure" --prefix=/ --disable-lb-wscreen-control $EXTRA_CONF \
        > configure.log 2>&1 || { tail -30 configure.log; exit 1; }
fi
if ! make -j"$JOBS" > make.log 2>&1; then
    grep -E -B2 -A6 '(error|Error)[: ]' make.log | head -80
    echo "BUILD FAILED (full log: $BDIR/make.log)"; exit 1
fi
if [ -n "$EXTRA_CFLAGS" ]; then
    # Instrument only the game sources; configure link probes lack the hooks
    ORIG_CFLAGS="$(sed -n 's/^CFLAGS = //p' src/Makefile)"
    if ! grep -q -- "$EXTRA_CFLAGS" src/.extra_cflags 2>/dev/null; then
        rm -f src/*.o src/syndwarsfx.exe
        echo "$EXTRA_CFLAGS" > src/.extra_cflags
    else
        # the plain make above rebuilt changed sources without the flags
        find src -maxdepth 1 -name '*.o' -newer src/.extra_cflags -delete
    fi
    if ! make -C src -j"$JOBS" CFLAGS="$ORIG_CFLAGS $EXTRA_CFLAGS" >> make.log 2>&1; then
        grep -E -B2 -A6 '(error|Error)[: ]' make.log | tail -40
        echo "BUILD FAILED (full log: $BDIR/make.log)"; exit 1
    fi
    touch src/.extra_cflags
fi
if [ "$VARIANT" = x64 ] && command -v ldd >/dev/null && [ -f src/syndwarsfx.exe ]; then
    ldd src/syndwarsfx.exe | awk '$3 ~ /^\/mingw64\// {print $3}' | xargs -r cp -u -t src/
fi
echo "BUILD OK: $BDIR/src"
if [ $CHECK -eq 1 ]; then
    if ! make -j"$JOBS" check > check.log 2>&1; then
        grep -E '^(FAIL|ERROR|# )' check.log | head -40
        echo "CHECK FAILED (log: $BDIR/check.log)"; exit 1
    fi
    grep -E '^# (TOTAL|PASS|FAIL)' check.log | head -20
    echo "CHECK OK"
fi
