#!/bin/bash
# run.sh LABEL: build ezo.yaml as ezo<LABEL> from TREE, flash a classic ESP32 devkit, capture 90 s and write
# out-ezo<LABEL>.events (bus traffic, callbacks, steps and sensor values with time stamps). Compare the events of dev
# and the PR: the replies, callback texts, read values and the delay before each read must match.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=ezo$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
printf 'substitutions:\n  name: %s\n\npackages:\n  ezo: !include ezo.yaml\n' "$NAME" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
$PY "$SC" "$P" 90 "$OUT/serial.txt" --reset > /dev/null 2>&1
[ -s "$OUT/serial.txt" ] || { echo "serial capture got nothing"; exit 1; }
grep -a -E "fakeezo|\[cb|\[step|ezo.sensor|\[sensor" "$OUT/serial.txt" | sed -E 's/^\[([0-9:.]+)\]/\1/' > "$R/out-$NAME.events"
CRASH=$(grep -a -c -i 'guru\|panic\|abort\|stack smashing' "$OUT/serial.txt")
echo "$NAME: $(wc -l < "$R/out-$NAME.events") event lines, $CRASH crash lines"
grep -a -q "step.*DONE" "$OUT/serial.txt" && [ "$CRASH" = 0 ] && echo PASS || { echo "FAIL: no DONE step or a crash"; exit 1; }
