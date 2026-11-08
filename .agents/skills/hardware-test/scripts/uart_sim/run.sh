#!/bin/bash
# usage: run.sh NAME TREE SECONDS OP...   flash over serial, capture serial, drive over the API
# Copy this, frdrive.py and the templates/uart_sim files into one scratch directory and run it there.
# env: PORT (the devkit serial port), SC (path to serial_capture.py), PY (checkout python); TREE is the esphome
# checkout to build from. ERASE=1 erases the flash first (stored preferences change the boot state); DRIVER names
# another API driver next to this script (default frdrive.py), called as DRIVER HOST LOG OP...
D=$(cd "$(dirname "$0")" && pwd); N=$1; T=$2; S=$3; shift 3; P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}
DRIVER=${DRIVER:-frdrive.py}
if [ -n "$ERASE" ]; then
  $PY -m esptool --port $P erase_flash > $D/$N-erase.log 2>&1 || { echo "erase failed"; exit 1; }
fi
(cd $T && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome run --no-logs $D/$N.yaml --device $P > $D/$N-upload.log 2>&1) || { echo "build or upload failed"; exit 1; }
$PY ${SC:?set SC} $P $S $D/$N-cap.txt > $D/$N-cap.err 2>&1 &
CAP=$!
trap 'kill $CAP 2>/dev/null' EXIT
UP=0
for i in $(seq 1 40); do $PY -c "import socket;socket.create_connection(('$N.local',6053),2)" 2>/dev/null && { UP=1; break; }; sleep 2; done
[ $UP = 1 ] || { echo "device $N.local not reachable on port 6053"; exit 1; }
cd $D && PYTHONUNBUFFERED=1 timeout $((S - 5)) $PY $DRIVER $N.local $N-api.log "$@" > $N-drive.txt 2>&1; RC=$?; echo "drive rc=$RC"
wait $CAP; CAPRC=$?; trap - EXIT
[ "$CAPRC" = 0 ] || { echo "serial capture failed (rc $CAPRC), crash check invalid"; exit 1; }
CRASH=$(grep -acE 'Guru|abort\(\)|rst:0x' $D/$N-cap.txt)
echo "crash lines: $CRASH"
[ "$RC" = 0 ] && [ "$CRASH" = 0 ]
