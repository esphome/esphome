#!/bin/bash
# run.sh LABEL: build outbtn<LABEL> for a D1 mini from TREE, flash it, capture 45 s and print the edge lines per case
# (high and low times of each pad from the press). Diff the outputs of dev and the PR.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=outbtn$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
printf 'esphome:\n  name: %s\n  includes:\n    - edges.hpp\nesp8266:\n  board: d1_mini\npackages:\n  ob: !include output-button-common.yaml\n' "$NAME" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
$PY "$SC" "$P" 45 "$OUT/serial.txt" --reset > /dev/null 2>&1
LC_ALL=C grep -a -E "\[edges|DONE" "$OUT/serial.txt" | sed -E 's/^\[[0-9:.]*\]\[.\]\[edges:[0-9]+\]: //' | tee "$R/out-$NAME.edges"
grep -q DONE "$R/out-$NAME.edges" || { echo "FAIL: no DONE"; exit 1; }
# Every case must show a rise and a fall on GPIO5; T3 also on GPIO4. The script runs each case 3 times.
for want in "T1 single 500ms GPIO5" "T2 repress at 250ms GPIO5" "T3 two outputs GPIO5" "T3 two outputs GPIO4" \
            "T4 b1000 then b500 at 300ms GPIO5"; do
  n=$(grep -F "$want:" "$R/out-$NAME.edges" | grep -F "+@" | grep -c -F -- "-@")
  [ "$n" -ge 3 ] || { echo "FAIL: $n of 3 '$want' lines have both edges"; exit 1; }
done
echo PASS
