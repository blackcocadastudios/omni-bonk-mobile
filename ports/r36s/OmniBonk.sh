#!/bin/bash
# Omni Bonk - launch script for PortMaster-style handhelds (R36S with dArkOS/ArkOS, ROCKNIX, AmberELEC...).
# The game reads the gamepad through SDL2 itself; gptokeyb is only started for the Select+Start kill switch.
# Everything printed here and by the game goes to ports/omnibonk/log.txt.

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

HAVE_PM=0
if [ -f "$controlfolder/control.txt" ]; then
  source "$controlfolder/control.txt"
  [ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
  type get_controls >/dev/null 2>&1 && get_controls
  HAVE_PM=1
fi

# the port folder sits next to this script (ports/omnibonk); PortMaster's $directory is the fallback
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -d "$SCRIPT_DIR/omnibonk" ]; then
  GAMEDIR="$SCRIPT_DIR/omnibonk"
elif [ -n "$directory" ] && [ -d "/$directory/ports/omnibonk" ]; then
  GAMEDIR="/$directory/ports/omnibonk"
else
  GAMEDIR="/roms/ports/omnibonk"
fi
cd "$GAMEDIR" || exit 1
mkdir -p "$GAMEDIR/conf"

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

echo "== Omni Bonk $(cat "$GAMEDIR/version.txt" 2>/dev/null) =="
echo "date: $(date)"
echo "portmaster: $HAVE_PM ($controlfolder) | cfw: ${CFW_NAME:-?} ${CFW_VERSION:-} | device: ${DEVICE_NAME:-?} | arch: ${DEVICE_ARCH:-$(uname -m)} | sticks: ${ANALOG_STICKS:-?}"
echo "kernel: $(uname -srm)"
echo "gamedir: $GAMEDIR"

[ -n "$sdl_controllerconfig" ] && export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

$ESUDO chmod +x "$GAMEDIR/omnibonk" 2>/dev/null

if [ -n "$GPTOKEYB" ]; then
  $GPTOKEYB "omnibonk" &
fi
type pm_platform_helper >/dev/null 2>&1 && pm_platform_helper "$GAMEDIR/omnibonk"

"$GAMEDIR/omnibonk" --handheld --device "${OMNIBONK_DEVICE:-R36S}" --data "$GAMEDIR/conf"
echo "exit code: $?"

if type pm_finish >/dev/null 2>&1; then
  pm_finish
else
  $ESUDO kill -9 "$(pidof gptokeyb)" 2>/dev/null
  $ESUDO systemctl restart oga_events 2>/dev/null &
  printf "\033c" > /dev/tty0 2>/dev/null
fi
