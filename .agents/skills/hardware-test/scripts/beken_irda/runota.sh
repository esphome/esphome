#!/bin/bash
# runota.sh NAME IP [SECONDS]: build NAME.yaml from TREE, OTA it to the BK72xx at IP, then capture SECONDS (default
# 120) of serial log to NAME-serial.txt (opening the port resets the chip) and score it with ana.py.
# Run it in a copy of templates/beken_irda. env: TREE, PORT, SC, PY as for run.sh
H=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; IP=${2:?device ip}; S=${3:-120}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
SC=${SC:?set SC}; PY=${PY:-$T/venv/bin/python}
PYTHONUNBUFFERED=1 PYTHONPATH=$T timeout 1500 $PY -m esphome run --no-logs "$N.yaml" --device "$IP" > "$N-ota.log" 2>&1 ||
  { echo "build or OTA failed"; tail -5 "$N-ota.log"; exit 1; }
sleep 3
$PY "$SC" "$P" "$S" "$N-serial.txt" > /dev/null 2>&1 || { echo "serial capture failed"; exit 1; }
echo "$N captured $(wc -l < "$N-serial.txt") lines"
$PY "$H/ana.py" "$N-serial.txt"
