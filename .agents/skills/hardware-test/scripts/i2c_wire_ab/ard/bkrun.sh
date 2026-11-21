#!/bin/bash
# bkrun.sh NAME PORT: flash NAME's UF2 to a BK72xx board (up to 3 tries), then capture 60 s from a reset (log on
# UART1) and print the sweep line count, the ALLOC_PROBE line and the SWDONE line. env: PY, LTPY (LibreTiny python)
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); cd "$D" || exit 1
N=${1:?name}; P=${2:?port}; PY=${PY:-python3}
LTPY=${LTPY:-$D/../pio/penv/.libretiny/bin/python}
[ -x "$LTPY" ] || LTPY=~/.platformio/penv/.libretiny/bin/python
for i in 1 2 3; do
  timeout 200 "$LTPY" "$S/flash_bk72xx.py" flash write -d "$P" ".esphome/build/$N/.pioenvs/$N/firmware.uf2" > "$N-flash.log" 2>&1 &&
    break
  echo "flash try $i failed: $(tail -1 "$N-flash.log")"
  [ "$i" = 3 ] && exit 1
done
$PY "$S/serial_capture.py" "$P" 60 "$N-ser.txt" --reset > /dev/null 2>&1
echo "$N $(grep -a -c 'SW ' "$N-ser.txt") $(grep -a -o 'ALLOC_PROBE.*' "$N-ser.txt") $(grep -a -o 'SWDONE.*' "$N-ser.txt")"
