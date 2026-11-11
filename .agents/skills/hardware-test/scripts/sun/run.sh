#!/bin/bash
# run.sh esp32|esp8266 LABEL: build a sun sweep firmware from TREE, flash it, capture up to 20 min (one up and down
# sweep), run check.py. secrets.yaml (wifi_ssid, wifi_password) goes next to sun-common.yaml; SNTP needs internet.
# env: TREE (esphome checkout to build from), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); K=${1:?esp32 or esp8266}; L=${2:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=sun$K$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
if [ "$K" = esp32 ]; then PLATFORM=$'esp32:\n  board: esp32dev\n  framework:\n    type: esp-idf'; else PLATFORM=$'esp8266:\n  board: d1_mini'; fi
printf 'esphome:\n  name: %s\n%s\npackages:\n  sun: !include sun-common.yaml\n' "$NAME" "$PLATFORM" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
$PY "$SC" "$P" 1200 "$OUT/serial.txt" --reset > /dev/null 2>&1
[ -s "$OUT/serial.txt" ] || { echo "serial capture got nothing"; exit 1; }
$PY "$R/check.py" "$OUT/serial.txt"
