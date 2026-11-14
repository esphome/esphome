#!/bin/bash
# run.sh NAME SECONDS: build NAME.yaml from TREE as device NAME, flash, then capture SECONDS of self-rebooting boots
# (each boot is a boot phase and a reconnect phase) into out-NAME/cap.txt and summarize them with an.py.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; S=${2:?seconds}
T=${TREE:?set TREE}; P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}
OUT=$R/out-$N; rm -rf "$OUT"; mkdir -p "$OUT"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome -s name "$N" run --no-logs "$R/$N.yaml" --device "$P" > "$OUT/upload.log" 2>&1) ||
  { echo "build or upload failed"; tail -5 "$OUT/upload.log"; exit 1; }
$PY "$SC" "$P" "$S" "$OUT/cap.txt" --ms > /dev/null 2>&1
[ -s "$OUT/cap.txt" ] || { echo "serial capture got nothing"; exit 1; }
$PY "$R/an.py" "$OUT/cap.txt"
