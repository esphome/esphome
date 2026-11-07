#!/bin/bash
# run.sh LABEL [SAMPLE_RATE] [CHANNEL]: build the loopback config as dc<LABEL> (default 16000 and stereo) from TREE,
# flash it over PORT, capture 60 s and tabulate the segment and settle lines with sum.py. Needs the three jumpers.
# env: TREE, PORT, SC (path to serial_capture.py), PY (checkout python)
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}
SC=${SC:?set SC}; NAME=dc$L; O=$D/out-$NAME; rm -rf "$O"; mkdir -p "$O"
printf 'substitutions:\n  sr: "%s"\n  ch: %s\nesphome:\n  name: %s\npackages:\n  common: !include loopback-common.yaml\n' \
  "${2:-16000}" "${3:-stereo}" "$NAME" > "$D/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$D/$NAME.yaml" --device "$P" > "$O/upload.log" 2>&1) || { echo "FAIL: build or upload"; exit 1; }
$PY "$SC" "$P" 60 "$O/serial.txt" > /dev/null 2>&1 || { echo "FAIL: capture got nothing"; exit 1; }
grep -a -E "SEG|SETTLE" "$O/serial.txt" | sed -E 's/^\[[0-9:]+\]\[I\]\[dc:[0-9]+\]: //' > "$O/res.txt"
RESETS=$(sed -n '/SEG mode/,$p' "$O/serial.txt" | grep -a -c -i -E 'Guru|abort|wdt|rst:')
echo "== $NAME $(grep -a -c 'SEQ DONE' "$O/serial.txt") done, $RESETS resets"
ST=; [ "${3:-stereo}" = stereo ] && ST=--stereo
python3 "$D/sum.py" $ST "$O/res.txt" || { echo "FAIL: missing segment measurements or no signal"; exit 1; }
grep -a -q 'SEQ DONE' "$O/serial.txt" || { echo "FAIL: the sequence never finished"; exit 1; }
[ "$RESETS" = 0 ] || { echo "FAIL: $RESETS crash or reset lines after the first segment"; exit 1; }
grep -a -q 'generator task not created\|i2s write failed' "$O/serial.txt" && { echo "FAIL: the generator did not run"; exit 1; }
echo PASS
