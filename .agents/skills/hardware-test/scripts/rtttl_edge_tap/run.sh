#!/bin/bash
# run.sh LABEL: build rtttl-edge-tap-dut.yaml from TREE as rtttl<LABEL>, flash the AUX-2, capture the eight tunes
# (one every 12 s after 20 s) and print the per-tune onset and release levels. Compare dev and the PR.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=rtttl$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
[ -f "$R/aux-2-pre-prod.yaml" ] || { echo "aux-2-pre-prod.yaml from NabuCasa/aux-2 must sit next to this script"; exit 1; }
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome -s dut_name "$NAME" run --no-logs "$R/rtttl-edge-tap-dut.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
$PY "$SC" "$P" 135 "$OUT/serial.txt" --reset > /dev/null 2>&1
[ -s "$OUT/serial.txt" ] || { echo "serial capture got nothing"; exit 1; }
grep -a -o -E "M play [0-9]+|notes=.*" "$OUT/serial.txt"
grep -a -q "M DONE" "$OUT/serial.txt" || { echo "FAIL: no M DONE"; exit 1; }
grep -a -q "notes=" "$OUT/serial.txt" || { echo "FAIL: no note reports (is rtttl writing to the tap?)"; exit 1; }
