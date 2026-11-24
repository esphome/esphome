#!/bin/bash
# flashdev.sh NAME YAML [SECS]: flash the end device on PORT with YAML (built as NAME from TREE) and capture its log to
# cap-NAME.txt for SECS (default 600) in the background. With CHIP set (esp32c6) the flash is erased first, so the
# device joins as a new one (the Zigbee network data lives in the nvs partition). The nRF dongle takes the upload on
# its app port (1200 baud touch DFU); set LOGPORT when its log comes up on another port. env: TREE, PY, PORT
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; Y=${2:?yaml}; SECS=${3:-600}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; P=${PORT:?set PORT}
pkill -f "[s]erial_capture.py $P" ; pkill -f "[s]erial_capture.py ${LOGPORT:-$P}" ; sleep 1
if [ -n "$CHIP" ]; then
  (cd "$T" && timeout 120 $PY -m esptool --chip "$CHIP" -p "$P" erase-flash > "$D/$N-erase.log" 2>&1) ||
    { echo "erase failed"; tail -3 "$D/$N-erase.log"; exit 1; }
fi
TREE=$T PY=$PY "$D/../pio_build.sh" upload "$N" "$Y" "$P" || exit 1
sleep 3
$PY "$D/../serial_capture.py" "${LOGPORT:-$P}" "$SECS" "$D/cap-$N.txt" ${CHIP:+--reset} > /dev/null 2>&1 &
echo "capture on ${LOGPORT:-$P}"
