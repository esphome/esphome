#!/bin/bash
# run.sh esp32|bk72xx LABEL: build rx-<kind>.yaml as rx<kind><LABEL> from TREE, flash it, capture 150 s, run check.py.
# The advertiser (advtrig.yaml) must already be running on a second ESP32 in range.
# env: TREE (esphome checkout to build from), PORT (receiver serial port), SC (path to serial_capture.py),
# PY (checkout python), BOARD (bk72xx board, default xh-wb3s), BKPY + BKFLASH (python and flash_bk72xx.py for
# bk72xx), ZERO_MAC (any or filter, what mac_address 00:00:00:00:00:00 should do on this build; default filter)
R=$(cd "$(dirname "$0")" && pwd); K=${1:?esp32 or bk72xx}; L=${2:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=rx$K$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
sed -e "s/\${name}/$NAME/" -e "s/\${board}/${BOARD:-xh-wb3s}/" "$R/rx-$K.yaml" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
if [ "$K" = esp32 ]; then
  (cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
  $PY "$SC" "$P" 150 "$OUT/serial.txt" --reset > /dev/null 2>&1
else
  (cd "$T" && $PY -m esphome compile "$R/$NAME.yaml" > "$OUT/compile.log" 2>&1) || { echo "build failed"; exit 1; }
  ${BKPY:?set BKPY} "${BKFLASH:?set BKFLASH}" flash write -d "$P" "$R/.esphome/build/$NAME/.pioenvs/$NAME/firmware.uf2" > "$OUT/flash.log" 2>&1 || { echo "flash failed"; exit 1; }
  $PY "$SC" "$P" 150 "$OUT/serial.txt" > /dev/null 2>&1
fi
[ -s "$OUT/serial.txt" ] || { echo "serial capture got nothing"; exit 1; }
grep -a "adverts" "$OUT/serial.txt" | tail -1
$PY "$R/check.py" "$OUT/serial.txt" --zero-mac "${ZERO_MAC:-filter}"
