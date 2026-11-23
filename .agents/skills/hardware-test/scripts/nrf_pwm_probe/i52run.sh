#!/bin/bash
# i52run.sh NAME [SECS]: flash NAME (i52.yaml built here with -s name NAME) to the ItsyBitsy nRF52840 by serial DFU,
# then capture SECS (default 45) and count the MEAS lines.
# env: NRF_SERIAL (the app's USB serial number, as ports.py prints it), DFU_PY (a Python with adafruit-nrfutil's
# nordicsemi package, such as ESPHome's sdk-nrf penv), PY (a Python with pyserial).
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); N=${1:?name}; T=${2:-45}
PY=${PY:?set PY}; SER=${NRF_SERIAL:?set NRF_SERIAL}; DFU=${DFU_PY:?set DFU_PY}
PKG=$D/.esphome/build/$N/.pioenvs/$N/firmware.zip
[ -f "$PKG" ] || { echo "FAIL: build $N first (no $PKG)"; exit 1; }
app() { $PY "$S/ports.py" --wait 2fe3:0100 --serial "$SER" --timeout "$1" 2>/dev/null; }
# 1200 baud touch: only an app built with dfu: true answers it
A=$(app 0) && $PY -c "import serial,sys; s=serial.Serial(sys.argv[1], 1200); s.close()" "$A" 2>/dev/null
B=$($PY "$S/ports.py" --wait 239a:0051 --timeout 30) || { echo "FAIL: no bootloader port; double tap RESET"; exit 1; }
sleep 1
$DFU -m nordicsemi.__main__ dfu serial -pkg "$PKG" -p "$B" -b 115200 --singlebank > "$D/$N-dfu.log" 2>&1 ||
  { tail -3 "$D/$N-dfu.log"; echo "FAIL: DFU failed; press RESET once and rerun"; exit 1; }
A=$(app 30) || { echo "FAIL: the app's USB port did not come back"; exit 1; }
$PY "$S/serial_capture.py" "$A" "$T" "$D/$N-cap.txt" > /dev/null 2>&1 || { echo "FAIL: no capture from $A"; exit 1; }
echo "MEAS lines: $(grep -a -c MEAS "$D/$N-cap.txt")"
