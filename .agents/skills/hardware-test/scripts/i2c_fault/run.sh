#!/bin/bash
# run.sh LABEL: build vlpf.yaml from TREE as vlpf<LABEL>, flash the ESP32-S3 with a VL53L0X on GPIO8 (SDA) and
# GPIO9 (SCL), capture the 150 s phase sequence (quiet, fault A, quiet, fault B, quiet) and count per phase.
# env: TREE (esphome checkout), PORT (S3 serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=vlpf$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
sed -E "s/^  name: vlpf$/  name: $NAME/; s/^  build_path: .*/  build_path: build-$NAME/" "$R/vlpf.yaml" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
$PY "$SC" "$P" 150 "$OUT/serial.txt" --reset > /dev/null 2>&1 || { echo "serial capture got nothing"; exit 1; }
python3 "$R/an.py" "$OUT/serial.txt"
