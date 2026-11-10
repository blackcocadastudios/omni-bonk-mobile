#!/usr/bin/env bash
# Smoke test of the real aarch64 binary. Runs inside an arm64 Debian 13 (trixie) container under QEMU
# (dArkOS is Debian-trixie based): Xvfb + Mesa llvmpipe stand in for the display and the Mali GPU.
# Usage: r36s_smoke.sh <package dir containing omnibonk/> <output dir>
set -x
PKG="${1:-/pkg}"; OUT="${2:-/out}"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends libsdl2-2.0-0 libegl1 libgles2 libgl1-mesa-dri libglx-mesa0 xvfb xauth > /dev/null
uname -m; ldd --version | head -1
cd "$PKG/omnibonk" || exit 1
ldd ./omnibonk.aarch64
mkdir -p "$OUT" /tmp/ob
export SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEO_X11_FORCE_EGL=1 SDL_VIDEO_FORCE_EGL=1
run() { # name, extra args...
  local name="$1"; shift
  timeout 900 xvfb-run -a -s "-screen 0 640x480x24" ./omnibonk.aarch64 --handheld --device R36S --data /tmp/ob --window 640x480 --no-audio --shot "$OUT/$name.bmp" "$@" 2>&1 | tee "$OUT/$name.log"
  echo "exit: ${PIPESTATUS[0]}" | tee -a "$OUT/$name.log"
}
run r36s_01_title --frames 30 --dt 0.033
run r36s_02_game_forest --demo 1,0 --frames 100 --dt 0.05
run r36s_03_game_desert --demo 3,1 --frames 100 --dt 0.05
ls -la "$OUT"
grep -h -E "GL renderer|GL version|init:|perf:|wrote|FATAL|exit:" "$OUT"/*.log
exit 0
