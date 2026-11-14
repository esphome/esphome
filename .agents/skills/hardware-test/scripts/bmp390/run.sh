#!/bin/bash
# run.sh NAME SECONDS [TAG]: build NAME.yaml (written by gen.py) from TREE, flash the ESP32-S3, capture SECONDS from
# a fresh boot into out-NAME/TAG.txt and print stats.py's summary.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; S=${2:?seconds}; G=${3:-run1}
T=${TREE:?set TREE}; P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}
[ -f "$R/$N.yaml" ] || { echo "no $N.yaml, run gen.py first"; exit 1; }
OUT=$R/out-$N; mkdir -p "$OUT"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$N.yaml" --device "$P" > "$OUT/upload.log" 2>&1) ||
  { echo "build or upload failed"; tail -5 "$OUT/upload.log"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
$PY "$SC" "$P" "$S" "$OUT/$G.txt" --reset > /dev/null 2>&1
[ -s "$OUT/$G.txt" ] || { echo "serial capture got nothing"; exit 1; }
$PY "$R/stats.py" "$OUT/$G.txt"
