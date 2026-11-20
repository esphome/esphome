#!/bin/bash
# rs.sh TAG NAME:PORT...: capture each already flashed board 250 s from a reset, publish to <name>/in every 2 s, stop
# the broker 35 s in and start it again 10 s later; the configs drop Wi-Fi at 60 s and 180 s. Writes NAME-rs.txt per
# board and rs-TAG-drv.txt (broker stop and start times) for rsan.py.
# env: PY (python with pyserial), MOSQUITTO (default mosquitto); the broker runs from this directory's mosq.conf.
D=$(cd "$(dirname "$0")" && pwd); S=$(cd "$D/../.." && pwd); G=${1:?tag}; shift
PY=${PY:?set PY}; MQ=${MOSQUITTO:-mosquitto}; DRV=$D/rs-$G-drv.txt; rm -f "$DRV"
names=(); caps=()
for b in "$@"; do
  n=${b%%:*}; p=${b#*:}; names+=("$n"); rm -f "$D/$n-rs.txt"
  "$PY" "$S/serial_capture.py" "$p" 250 "$D/$n-rs.txt" --reset --ms > /dev/null 2>&1 &
  caps+=($!)
done
(
  i=0; end=$((SECONDS + 250))
  while [ $SECONDS -lt $end ]; do
    i=$((i + 1))
    for n in "${names[@]}"; do mosquitto_pub -p 18843 -t "$n/in" -m "p$i" 2> /dev/null; done
    sleep 2
  done
) &
pub=$!
sleep 35; pkill -f "[m]osquitto -c $D/mosq.conf"; echo "$(date +%H:%M:%S).000 BROKER STOP" >> "$DRV"
sleep 10; (exec "$MQ" -c "$D/mosq.conf" >> "$D/mosq.log" 2>&1 < /dev/null) &
echo "$(date +%H:%M:%S).000 BROKER START" >> "$DRV"
sleep 210; kill $pub 2> /dev/null; wait "${caps[@]}"
echo RSDONE
