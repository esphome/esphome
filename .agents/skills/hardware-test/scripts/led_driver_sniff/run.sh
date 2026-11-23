#!/bin/bash
# run.sh LABEL: build the capture config (gen.py's led.yaml) from TREE as led<LABEL>, flash a classic ESP32 devkit,
# capture until DONE and check the selftest. Then compare two runs per driver:
#   python3 compare.py out-led<BASE>/serial.txt out-led<PR>/serial.txt bp5758d
# env: TREE (esphome checkout), PORT (devkit serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=led$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
(cd "$R" && python3 gen.py) || { echo "gen.py failed"; exit 1; }  # always, so an edited gen.py is used
printf 'substitutions:\n  name: %s\n\npackages:\n  led: !include led.yaml\n' "$NAME" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
$PY "$SC" "$P" 75 "$OUT/serial.txt" --reset > /dev/null 2>&1
[ -s "$OUT/serial.txt" ] || { echo "serial capture got nothing"; exit 1; }
echo "$(grep -a -c 'CAP ' "$OUT/serial.txt") windows, DONE $(grep -a -c DONE "$OUT/serial.txt")"
grep -a -q DONE "$OUT/serial.txt" || { echo "FAIL: no DONE"; exit 1; }
python3 "$R/selftest.py" "$OUT/serial.txt"
