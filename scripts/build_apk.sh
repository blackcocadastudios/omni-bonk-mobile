#!/usr/bin/env bash
# Builds build/OmniBonk.apk without Gradle:
#   NDK + CMake (libomnibonk.so per ABI) -> aapt2 (manifest/resources) -> javac + d8 (MainActivity)
#   -> zip -> zipalign -> apksigner
# Needs: ANDROID_HOME (SDK with build-tools + a platform >= 34 + NDK), JDK 17+, cmake, zip.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build/android"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
[ -n "$SDK" ] && [ -d "$SDK" ] || { echo "ANDROID_HOME/ANDROID_SDK_ROOT not set"; exit 1; }

NDK="${ANDROID_NDK_LATEST_HOME:-${ANDROID_NDK_HOME:-${ANDROID_NDK:-}}}"
if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then NDK="$(ls -d "$SDK"/ndk/* 2>/dev/null | sort -V | tail -1 || true)"; fi
[ -d "$NDK" ] || { echo "Android NDK not found"; exit 1; }

BT="$(ls -d "$SDK"/build-tools/* | sort -V | tail -1)"
PLATFORM="$(ls -d "$SDK"/platforms/android-* | grep -E 'android-[0-9]+$' | sort -V | tail -1)"
ANDROID_JAR="$PLATFORM/android.jar"
ABIS="${ABIS:-arm64-v8a armeabi-v7a x86_64}"
MIN_SDK=26
TARGET_SDK=34
VERSION_CODE="${VERSION_CODE:-1}"
VERSION_NAME="${VERSION_NAME:-1.0.0}"
KS="${KEYSTORE:-$ROOT/android/omnibonk-test.keystore}"
KS_PASS="${KEYSTORE_PASS:-omnibonk}"
KS_ALIAS="${KEY_ALIAS:-omnibonk}"
KEY_PASS="${KEY_PASS:-$KS_PASS}"
GEN="Unix Makefiles"; command -v ninja >/dev/null 2>&1 && GEN="Ninja"
JOBS="$(nproc 2>/dev/null || echo 4)"

echo "SDK=$SDK"; echo "NDK=$NDK"; echo "build-tools=$BT"; echo "platform=$PLATFORM"; echo "ABIs=$ABIS"

rm -rf "$OUT"
mkdir -p "$OUT/apk" "$OUT/flat" "$OUT/classes" "$OUT/dex"

# 1) native library for every ABI
STRIP="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip"
for ABI in $ABIS; do
  echo "== native $ABI"
  cmake -S "$ROOT" -B "$OUT/cmake-$ABI" -G "$GEN" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="android-$MIN_SDK" -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release > "$OUT/cmake-$ABI.log" 2>&1 || { cat "$OUT/cmake-$ABI.log"; exit 1; }
  cmake --build "$OUT/cmake-$ABI" -j"$JOBS"
  mkdir -p "$OUT/apk/lib/$ABI"
  "$STRIP" --strip-unneeded -o "$OUT/apk/lib/$ABI/libomnibonk.so" "$OUT/cmake-$ABI/libomnibonk.so"
  ls -la "$OUT/apk/lib/$ABI/libomnibonk.so"
done

# 2) resources + manifest
echo "== aapt2"
find "$ROOT/android/res" -type f | sort | while read -r f; do "$BT/aapt2" compile "$f" -o "$OUT/flat/"; done
"$BT/aapt2" link -o "$OUT/base.apk" -I "$ANDROID_JAR" \
  --manifest "$ROOT/android/AndroidManifest.xml" \
  --min-sdk-version "$MIN_SDK" --target-sdk-version "$TARGET_SDK" \
  --version-code "$VERSION_CODE" --version-name "$VERSION_NAME" \
  "$OUT"/flat/*.flat

# 3) Java shell (NativeActivity subclass) -> classes.dex
echo "== javac + d8"
javac -source 1.8 -target 1.8 -Xlint:-options -encoding UTF-8 \
  -bootclasspath "$ANDROID_JAR" -classpath "$ANDROID_JAR" -d "$OUT/classes" \
  $(find "$ROOT/android/java" -name '*.java')
"$BT/d8" --release --min-api "$MIN_SDK" --lib "$ANDROID_JAR" --output "$OUT/dex" $(find "$OUT/classes" -name '*.class')
cp "$OUT/dex/classes.dex" "$OUT/apk/"

# 4) assemble, align, sign
echo "== package"
cp "$OUT/base.apk" "$OUT/unaligned.apk"
(cd "$OUT/apk" && zip -q -r -9 "$OUT/unaligned.apk" classes.dex lib)
"$BT/zipalign" -f -p 4 "$OUT/unaligned.apk" "$OUT/aligned.apk"
mkdir -p "$ROOT/build"
"$BT/apksigner" sign --ks "$KS" --ks-pass "pass:$KS_PASS" --ks-key-alias "$KS_ALIAS" --key-pass "pass:$KEY_PASS" \
  --out "$ROOT/build/OmniBonk.apk" "$OUT/aligned.apk"
"$BT/apksigner" verify --verbose "$ROOT/build/OmniBonk.apk"
"$BT/aapt2" dump badging "$ROOT/build/OmniBonk.apk" > "$OUT/badging.txt" 2>&1 || true
head -5 "$OUT/badging.txt" || true
ls -la "$ROOT/build/OmniBonk.apk"
echo "APK: $ROOT/build/OmniBonk.apk"
