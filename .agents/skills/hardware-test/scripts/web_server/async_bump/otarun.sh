#!/bin/bash
# otarun.sh NAME IP: native OTA of NAME (from TREE) to a board reached only over the network, then flow.py and the
# web OTA, with an API log capture throughout. env: TREE, PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); N=${1:?name}; IP=${2:?ip}; T=${TREE:?set TREE}
PY=${PY:-$T/venv/bin/python}
PY=$PY PIO_CORE=${PIO_CORE:-$D/pio} bash "$S/pio_build.sh" upload "$N" "$D/$N.yaml" "$IP" || exit 1
for _ in $(seq 1 60); do curl -s -m 3 -o /dev/null "http://$IP/" && break; sleep 3; done
(cd "$T" && PYTHONPATH=$T PYTHONUNBUFFERED=1 timeout 1200 $PY -m esphome logs "$D/$N.yaml" --device "$IP" \
  > "$D/$N-apilog.txt" 2>&1 &)
sleep 10
$PY -I "$D/flow.py" "$IP" "$D/$N"
B=$D/.esphome/build/$N/.pioenvs/$N; IMG=$B/firmware.bin; [ -f "$B/firmware.uf2" ] && IMG=$B/firmware.uf2
PY=$PY bash "$D/webota.sh" "$IP" "$IMG" "$D/$N"; RC=$?
sleep 5; pkill -f "[e]sphome logs $D/$N.yaml"
echo "APILOG crash lines $(grep -a -c -E 'CRASH|Crash|abort|HardFault|assert' "$D/$N-apilog.txt")," \
  "boots seen $(grep -a -c 'ESPHome version' "$D/$N-apilog.txt"), lines $(wc -l < "$D/$N-apilog.txt")"
exit $RC
