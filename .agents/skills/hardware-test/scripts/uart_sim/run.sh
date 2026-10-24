#!/bin/zsh
# usage: run.sh NAME TREE SECONDS OP...   flash over serial, capture serial, drive over the API
# Copy this, frdrive.py and the templates/uart_sim files into one scratch directory and run it there.
# env: PORT (the devkit serial port), SC (path to serial_capture.py); TREE is the esphome checkout to build from
D=${0:A:h}; N=$1; T=$2; S=$3; shift 3; P=${PORT:?set PORT}; PY=$T/venv/bin/python
(cd $T && PYTHONUNBUFFERED=1 timeout 300 venv/bin/python -m esphome upload $D/$N.yaml --device $P > $D/$N-upload.log 2>&1) || { echo "upload failed"; exit 1; }
$PY ${SC:?set SC} $P $S $D/$N-cap.txt > $D/$N-cap.err 2>&1 &
CAP=$!
for i in $(seq 1 40); do $PY -c "import socket;socket.create_connection(('$N.local',6053),2)" 2>/dev/null && break; sleep 2; done
cd $D && PYTHONUNBUFFERED=1 timeout $((S - 5)) $PY frdrive.py $N.local $N-api.log "$@" > $N-drive.txt 2>&1; echo "drive rc=$?"
wait $CAP
grep -acE 'Guru|abort\(\)|rst:0x' $D/$N-cap.txt
