#!/bin/bash
# run.sh LABEL: build bench.yaml as hb<LABEL> from TREE, flash it over PORT, capture 25 s and print the cycle counts.
# Fails when a parser result differs from Bluedroid's sscanf parsing or the bench never finished.
# env: TREE (checkout with #22081), PORT, SC (path to serial_capture.py), PY (checkout python), BOARD (default esp32dev)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; OUT=$R/out-hb$L; rm -rf "$OUT"; mkdir -p "$OUT"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome -s name "hb$L" -s board "${BOARD:-esp32dev}" run --no-logs "$R/bench.yaml" --device "$P" \
  > "$OUT/upload.log" 2>&1) || { echo "FAIL: build or upload"; exit 1; }
$PY "$SC" "$P" 25 "$OUT/serial.txt" --reset > /dev/null 2>&1 || { echo "FAIL: capture got nothing"; exit 1; }
grep -a '\[bench' "$OUT/serial.txt" | sed 's/.*\]: //'
[ "$(grep -a -c 'DONE mismatches=0' "$OUT/serial.txt")" -ge 2 ] || { echo "FAIL: missing DONE or a mismatch"; exit 1; }
echo PASS
