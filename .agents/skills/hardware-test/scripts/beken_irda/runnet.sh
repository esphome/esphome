#!/bin/bash
# runnet.sh NAME IP [SECONDS]: build NAME.yaml from TREE, OTA it to the board at IP (three tries) and read its log over
# the API for SECONDS (default 130) into NAME-net.txt, then score it with ana.py. For boards with no serial
# access (the cb*.yaml builds). Run it in a copy of templates/beken_irda. env: TREE, PY as for run.sh
H=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; IP=${2:?device ip}; S=${3:-130}; T=${TREE:?set TREE}
PY=${PY:-$T/venv/bin/python}
export PYTHONUNBUFFERED=1 PYTHONPATH=$T
for i in 1 2 3; do
  timeout 1500 $PY -m esphome run --no-logs "$N.yaml" --device "$IP" > "$N-ota.log" 2>&1 && break
  echo "OTA try $i failed"; sleep 10
done
grep -q "OTA successful" "$N-ota.log" || { echo "OTA failed"; tail -5 "$N-ota.log"; exit 1; }
timeout "$S" $PY -m esphome logs "$N.yaml" --device "$IP" > "$N-net.txt" 2>&1
echo "$N got $(wc -l < "$N-net.txt") lines"
$PY "$H/ana.py" "$N-net.txt"
