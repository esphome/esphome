#!/bin/bash
# run.sh LABEL: build color-correct.yaml from TREE as colorcorrect<LABEL>, flash the Pico 2 W, capture 160 s (two
# rounds of every case) and print the RESULT and DONE counts. Compare two builds with
# `python analyze.py out-colorcorrectBASE/serial.txt out-colorcorrectPR/serial.txt`.
# env: TREE (esphome checkout), PORT (Pico serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=colorcorrect$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome -s name "$NAME" run --no-logs "$R/color-correct.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
# USB CDC: no reset line, the capture follows the port back after the flash reboot
$PY "$SC" "$P" 160 "$OUT/serial.txt" > /dev/null 2>&1
[ -s "$OUT/serial.txt" ] || { echo "serial capture got nothing"; exit 1; }
echo "$NAME: $(grep -a -c "RESULT " "$OUT/serial.txt") results, $(grep -a -c "IN r" "$OUT/serial.txt") cases"
grep -a -q DONE "$OUT/serial.txt" || { echo "FAIL: no DONE"; exit 1; }
