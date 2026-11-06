#!/bin/bash
# run.sh NAME TREE [SECONDS]: build NAME.yaml from TREE, flash it over serial, capture SECONDS (default 45) of serial
# log to NAME-cap.txt. The case script runs by itself from on_boot; fails when the capture has no "CASE done" line.
# env: PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; T=${2:?tree}; S=${3:-45}; P=${PORT:?set PORT}; SC=${SC:?set SC}
PY=${PY:-$T/venv/bin/python}
(cd "$D" && PYTHONUNBUFFERED=1 PYTHONPATH=$T timeout 1500 $PY -m esphome upload "$N.yaml" --device "$P" > "$N-upload.log" 2>&1) ||
  { echo "upload failed"; tail -5 "$D/$N-upload.log"; exit 1; }
$PY "$SC" "$P" "$S" "$D/$N-cap.txt" --reset > /dev/null 2>&1 || { echo "serial capture failed"; exit 1; }
echo "$N $(wc -l < "$D/$N-cap.txt") lines, crash lines $(LC_ALL=C grep -a -c -E 'Guru|abort\(\)' "$D/$N-cap.txt")"
grep -a -q "CASE done" "$D/$N-cap.txt" || { echo "FAIL: no CASE done line"; exit 1; }
