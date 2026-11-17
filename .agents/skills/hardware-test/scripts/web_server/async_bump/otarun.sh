#!/bin/bash
# otarun.sh NAME IP: native OTA of NAME (from TREE) to a board reached only over the network, then flow.py and the
# web OTA, with an API log capture throughout. env: TREE, PY
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; IP=${2:?ip}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
(cd "$T" && PLATFORMIO_CORE_DIR=${PIO_CORE:-$D/pio} PYTHONPATH=$T PYTHONUNBUFFERED=1 timeout 1500 \
  $PY -m esphome upload "$D/$N.yaml" --device "$IP") > "$D/$N-upload.log" 2>&1
echo "upload rc=$?"; grep -a -E 'OTA successful|ERROR' "$D/$N-upload.log" | tail -2
for _ in $(seq 1 60); do curl -s -m 3 -o /dev/null "http://$IP/" && break; sleep 3; done
(cd "$T" && PYTHONPATH=$T PYTHONUNBUFFERED=1 timeout 1200 $PY -m esphome logs "$D/$N.yaml" --device "$IP" \
  > "$D/$N-apilog.txt" 2>&1 &)
sleep 10
$PY -I "$D/flow.py" "$IP" "$D/$N"
bash "$D/webota.sh" "$IP" "$D/.esphome/build/$N/.pioenvs/$N/firmware.bin" "$D/$N"
sleep 5; pkill -f "[e]sphome logs $D/$N.yaml"
echo "APILOG crash lines $(grep -a -c -E 'CRASH|Crash|abort|HardFault|assert' "$D/$N-apilog.txt")," \
  "boots seen $(grep -a -c 'ESPHome version' "$D/$N-apilog.txt"), lines $(wc -l < "$D/$N-apilog.txt")"
