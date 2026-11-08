#!/bin/bash
# flashcap.sh DIR NAME TREE SECONDS: upload the prebuilt NAME from DIR over serial, then capture a fresh boot to
# DIR/NAME-cap.txt (ms time stamps). Fails when the capture has no DONE line or shows a crash.
# env: PORT (serial port), SC (path to serial_capture.py), PY (default TREE/venv/bin/python)
H=$(cd "$(dirname "$0")" && pwd); N=${2:?name}; S=${4:?seconds}
D=$(cd "${1:?dir}" && pwd) || exit 1; T=$(cd "${3:?tree}" && pwd) || exit 1
P=${PORT:?set PORT}; SC=${SC:?set SC}; PY=${PY:-$T/venv/bin/python}
cd "$D" && PYTHONPATH=$T PYTHONUNBUFFERED=1 timeout 300 $PY -m esphome upload "$D/$N.yaml" --device "$P" > "$D/$N-upload.log" 2>&1 ||
  { echo "upload failed"; tail -5 "$D/$N-upload.log"; exit 1; }
$PY "$SC" "$P" "$S" "$D/$N-cap.txt" --reset --ms > "$D/$N-cap.err" 2>&1 || { echo "serial capture failed"; exit 1; }
C=$D/$N-cap.txt
CRASH=$(LC_ALL=C grep -a -c -E 'Guru|abort\(\)|Backtrace:|task_wdt: Task' "$C")
[ "$CRASH" = 0 ] || { echo "FAIL: $CRASH crash lines in $C"; exit 1; }
DONE=$(grep -a -m1 -o 'DONE .*' "$C") || { echo "FAIL: no DONE line in $C (raise SECONDS?)"; exit 1; }
echo "$N: $DONE"
$PY "$H/check.py" "$C"
