#!/bin/bash
# run-full.sh NAME TREE [SECONDS]: erase the flash, build NAME.yaml from TREE and flash it over serial, capture
# SECONDS (default 100) of serial log to NAME-cap.txt and drive every entity over the API with ld6drive.py.
# The erase matters: switch and select states and the area id are stored preferences.
# env: PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; T=${2:?tree}; S=${3:-100}; P=${PORT:?set PORT}; SC=${SC:?set SC}
PY=${PY:-$T/venv/bin/python}
$PY -m esptool --port "$P" erase_flash > "$D/$N-erase.log" 2>&1 || { echo "erase failed"; exit 1; }
(cd "$D" && PYTHONUNBUFFERED=1 PYTHONPATH=$T timeout 1500 $PY -m esphome run --no-logs "$N.yaml" --device "$P" > "$N-upload.log" 2>&1) ||
  { echo "upload failed"; tail -5 "$D/$N-upload.log"; exit 1; }
$PY "$SC" "$P" "$S" "$D/$N-cap.txt" --reset > /dev/null 2>&1 &
CAP=$!
trap 'kill $CAP 2>/dev/null' EXIT
UP=0
for i in $(seq 1 40); do $PY -c "import socket;socket.create_connection(('$N.local',6053),2)" 2>/dev/null && { UP=1; break; }; sleep 2; done
[ $UP = 1 ] || { echo "device $N.local not reachable on port 6053"; exit 1; }
$PY "$D/ld6drive.py" "$N.local" > "$D/$N-drive.txt" 2>&1; RC=$?; echo "drive rc=$RC"
wait $CAP; CAPRC=$?; trap - EXIT
[ "$CAPRC" = 0 ] || { echo "serial capture failed (rc $CAPRC)"; exit 1; }
CRASH=$(LC_ALL=C grep -a -c -E 'Guru|abort\(\)' "$D/$N-cap.txt")
echo "$N $(wc -l < "$D/$N-cap.txt") serial lines, crash lines $CRASH"
grep -q "^FINAL" "$D/$N-drive.txt" || { echo "FAIL: ld6drive.py did not finish"; exit 1; }
[ "$RC" = 0 ] && [ "$CRASH" = 0 ]
