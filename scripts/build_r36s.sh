#!/usr/bin/env bash
# Cross-compiles the SDL2 port for aarch64 Linux handhelds (R36S & other RK3326 devices) and packages
# build/OmniBonk-R36S.zip in the PortMaster layout (OmniBonk.sh + omnibonk/).
#
# Meant to run in a Debian 11 (bullseye) environment so the binary only needs glibc 2.31:
#   dpkg --add-architecture arm64 && apt-get update
#   apt-get install -y cmake ninja-build zip file g++-aarch64-linux-gnu libsdl2-dev:arm64
# (the CI job uses the debian:bullseye container; `docker run -v $PWD:/src -w /src debian:bullseye ...` works too)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build/r36s"
CROSS="${CROSS:-aarch64-linux-gnu-}"
SDL_INC="${SDL2_INCLUDE_DIR:-/usr/include/SDL2}"
SDL_LIB="${SDL2_LIBRARY:-/usr/lib/aarch64-linux-gnu/libSDL2.so}"
VERSION="${VERSION_NAME:-1.1.0}"
GEN="Unix Makefiles"; command -v ninja >/dev/null 2>&1 && GEN="Ninja"

[ -f "$SDL_INC/SDL.h" ] || { echo "SDL2 headers not found in $SDL_INC"; exit 1; }
[ -e "$SDL_LIB" ] || { echo "arm64 libSDL2 not found at $SDL_LIB"; exit 1; }

rm -rf "$OUT"; mkdir -p "$OUT"
cmake -S "$ROOT" -B "$OUT/cmake" -G "$GEN" \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER="${CROSS}gcc" -DCMAKE_CXX_COMPILER="${CROSS}g++" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-mtune=cortex-a35" \
  -DOMNIBONK_SDL=ON -DSDL2_INCLUDE_DIR="$SDL_INC" -DSDL2_LIBRARY="$SDL_LIB" > "$OUT/cmake.log" 2>&1 || { cat "$OUT/cmake.log"; exit 1; }
cmake --build "$OUT/cmake" -j"$(nproc 2>/dev/null || echo 4)"

BIN="$OUT/cmake/omnibonk"
"${CROSS}strip" --strip-unneeded "$BIN"
file "$BIN"
echo "== shared library dependencies"
"${CROSS}readelf" -d "$BIN" | grep NEEDED
echo "== newest glibc symbol version required"
"${CROSS}objdump" -T "$BIN" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1
if "${CROSS}readelf" -d "$BIN" | grep -E 'NEEDED.*(libstdc\+\+|libGL|libEGL)'; then echo "unexpected dependency"; exit 1; fi

# package: OmniBonk.sh + omnibonk/ (extract into ports/)
PKG="$OUT/pkg"
mkdir -p "$PKG"
cp -r "$ROOT/ports/r36s/." "$PKG/"
cp "$BIN" "$PKG/omnibonk/omnibonk"
chmod +x "$PKG/OmniBonk.sh" "$PKG/omnibonk/omnibonk"
echo "$VERSION ($(date -u +%Y-%m-%d))" > "$PKG/omnibonk/version.txt"
cp "$ROOT/README.md" "$PKG/omnibonk/README.md"
rm -f "$ROOT/build/OmniBonk-R36S.zip"
(cd "$PKG" && zip -q -r -9 "$ROOT/build/OmniBonk-R36S.zip" OmniBonk.sh omnibonk)
ls -la "$ROOT/build/OmniBonk-R36S.zip"
unzip -l "$ROOT/build/OmniBonk-R36S.zip"
