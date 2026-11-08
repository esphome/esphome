#!/bin/bash
# run.sh NAME TREE [SECONDS]: build NAME.yaml from TREE, flash it over serial, capture SECONDS (default 45) of serial
# log to NAME-cap.txt. The case script runs by itself from on_boot; fails on a crash or without a "CASE done" line.
# env: PORT (serial port), SC (path to serial_capture.py), PY (checkout python), ERASE=1 erases the flash first
# (stored preferences change the boot state; m5 needs it and SECONDS of 70 or more)
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; T=${2:?tree}; S=${3:-45}; P=${PORT:?set PORT}; SC=${SC:?set SC}
PY=${PY:-$T/venv/bin/python}
if [ -n "$ERASE" ]; then
  $PY -m esptool --port "$P" erase_flash > "$D/$N-erase.log" 2>&1 || { echo "erase failed"; exit 1; }
fi
(cd "$D" && PYTHONUNBUFFERED=1 PYTHONPATH=$T timeout 1500 $PY -m esphome run --no-logs "$N.yaml" --device "$P" > "$N-upload.log" 2>&1) ||
  { echo "build or upload failed"; tail -5 "$D/$N-upload.log"; exit 1; }
$PY "$SC" "$P" "$S" "$D/$N-cap.txt" --reset > /dev/null 2>&1 || { echo "serial capture failed"; exit 1; }
CRASH=$(LC_ALL=C grep -a -c -E 'Guru|abort\(\)|Backtrace:|task_wdt: Task' "$D/$N-cap.txt")
echo "$N $(wc -l < "$D/$N-cap.txt") lines, crash lines $CRASH"
[ "$CRASH" = 0 ] || { echo "FAIL: crash in $D/$N-cap.txt"; exit 1; }
grep -a -q "CASE done" "$D/$N-cap.txt" || { echo "FAIL: no CASE done line"; exit 1; }
