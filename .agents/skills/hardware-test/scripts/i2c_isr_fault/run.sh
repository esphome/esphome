#!/bin/bash
# run.sh LABEL [SECONDS]: build example.yaml from TREE as i2cfault-<LABEL> (edit its substitutions for the framework,
# seed and fault rate), flash the ESP32, capture SECONDS (default 640) from a reset and summarize.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; S=${2:-640}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=i2cfault-$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
sed -E "s/^  name: .*/  name: $NAME/" "$R/example.yaml" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
$PY "$SC" "$P" "$S" "$OUT/serial.txt" --reset > /dev/null 2>&1 || { echo "serial capture got nothing"; exit 1; }
"$R/summary.sh" "$OUT/serial.txt"
