#!/bin/bash
# run.sh LABEL: build can.yaml as can<LABEL> from TREE, flash a classic ESP32 devkit (no CAN transceiver: the TWAI
# receive input is routed to its own transmit pad), capture 30 s and write out-can<LABEL>.counts, the number of
# on_frame fires per trigger, id, flags and data. Diff the counts of dev and the PR.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=can$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
printf 'substitutions:\n  name: %s\n\npackages:\n  can: !include can.yaml\n' "$NAME" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
$PY "$SC" "$P" 30 "$OUT/serial.txt" --reset > /dev/null 2>&1
[ -s "$OUT/serial.txt" ] || { echo "serial capture got nothing"; exit 1; }
grep -a -o '\[of:[0-9]*\]: .*' "$OUT/serial.txt" | sed 's/\[of:[0-9]*\]: //' | sort | uniq -c > "$R/out-$NAME.counts"
grep -a "canself.*DONE" "$OUT/serial.txt" | tail -1
grep -a -q "canself.*DONE sent [1-9][0-9]* failed 0" "$OUT/serial.txt" && [ -s "$R/out-$NAME.counts" ] && echo PASS \
  || { echo "FAIL: no DONE, a failed send or no on_frame fires"; exit 1; }
