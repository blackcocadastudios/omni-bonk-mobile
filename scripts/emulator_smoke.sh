#!/usr/bin/env bash
# CI smoke test (runs inside reactivecircus/android-emulator-runner):
# installs the APK, opens the title screen, then starts bot-played demo runs
# ("args" intent extra) and saves screenshots + logcat to build/emu/.
set -x
PKG=com.blackcocada.omnibonk
OUT=build/emu
mkdir -p "$OUT"

adb wait-for-device
adb shell getprop ro.build.version.release
adb shell getprop ro.product.cpu.abi
adb shell wm size 720x1600 || true
adb shell wm density 280 || true
adb shell settings put system accelerometer_rotation 0 || true
adb install -r build/OmniBonk.apk || { echo "INSTALL FAILED"; exit 1; }
adb logcat -G 16M || true
adb logcat -c || true

shot() { adb exec-out screencap -p > "$OUT/$1.png"; ls -la "$OUT/$1.png"; }
alive() { if adb shell pidof "$PKG" >/dev/null; then echo "process alive ($1)"; else echo "PROCESS NOT RUNNING ($1)"; fi; }

adb shell am start -W -n "$PKG/.MainActivity"
sleep 30
shot emu_01_title
alive titulo

run_demo() { # $1 = name, $2 = "char,map", $3 = seconds before the first shot
  adb shell am force-stop "$PKG"
  sleep 2
  adb shell am start -W -n "$PKG/.MainActivity" --es args "demo=$2"
  sleep "$3"
  shot "$1"
  alive "$1"
}
run_demo emu_02_game_forest_ninja 1,0 50
sleep 40
shot emu_03_game_forest_ninja_b
run_demo emu_04_game_desert_monke 3,1 60
run_demo emu_05_game_graveyard_calcium 2,2 60

# touch flow on a 1600x720 landscape screen (UI scale 3): PLAY -> NEXT -> START!, then the joystick
adb shell am force-stop "$PKG"
sleep 2
adb shell am start -W -n "$PKG/.MainActivity"
sleep 25
adb shell input tap 800 353
sleep 5
shot emu_06_touch_characters
adb shell input tap 1396 669
sleep 5
shot emu_07_touch_map_time
adb shell input tap 1396 669
sleep 20
shot emu_08_touch_game
adb shell input swipe 250 560 250 470 4000
shot emu_09_touch_joystick
alive toque

adb shell dumpsys gfxinfo "$PKG" > "$OUT/gfxinfo.txt" 2>&1 || true
adb logcat -d > "$OUT/logcat_full.txt" 2>&1 || true
grep -E "OmniBonk|AndroidRuntime|DEBUG|libc|FATAL|EGL|GLES|aaudio" "$OUT/logcat_full.txt" | tail -400 > "$OUT/logcat_omnibonk.txt" || true
tail -100 "$OUT/logcat_omnibonk.txt"
echo "== screen transitions"
grep -E "OmniBonk: (screen|launch args|fps)" "$OUT/logcat_full.txt" | tail -40 || true
if grep -q "screen MAP -> GAME" "$OUT/logcat_full.txt"; then echo "TOUCH FLOW OK (title -> chars -> map -> game)"; else echo "TOUCH FLOW DID NOT REACH THE GAME"; fi
exit 0
