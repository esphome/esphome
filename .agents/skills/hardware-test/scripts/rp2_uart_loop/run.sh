#!/bin/bash
# run.sh NAME YAML USBSERIAL: upload YAML as NAME from TREE over the 1200 baud touch to the Pico with that USB serial
# number, wait for it to come back, capture 30 s and count the CHOICE, LOOP and SETDONE lines.
# env: TREE, PY (default $TREE/venv/bin/python), PIO_CORE (optional)
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); N=${1:?name}; Y=${2:?yaml}; SER=${3:?usb serial number}
PY=${PY:-${TREE:?set TREE}/venv/bin/python}
P=$($PY "$S/ports.py" --serial "$SER" --timeout 10) || { echo "no port for $SER"; exit 1; }
PIO_CORE=${PIO_CORE:-$D/pio} bash "$S/pio_build.sh" upload "$N" "$Y" "$P" || exit 1
P=$($PY "$S/ports.py" --wait 2e8a --serial "$SER" --timeout 30) || { echo "$SER did not come back"; exit 1; }
$PY "$S/serial_capture.py" "$P" 30 "$D/$N-cap.txt" > "$D/$N-cap.err" 2>&1 || { echo "capture failed (see $D/$N-cap.err)"; exit 1; }
grep -a -E "CHOICE|LOOP|SETDONE|SerialPIO|Hardware Serial|hardware serial|can only|taken by" "$D/$N-cap.txt" |
  sed 's/^\[[0-9:.]*\]//' | sort | uniq -c
grep -aq SETDONE "$D/$N-cap.txt" || { echo "no SETDONE line"; exit 1; }
