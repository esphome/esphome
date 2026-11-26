#!/bin/bash
# rs.sh TAG NAME:PORT...: capture each already flashed board 250 s from a reset, publish to <name>/in every 2 s, stop
# the broker 35 s in and start it again 10 s later; the configs drop Wi-Fi at 60 s and 180 s. Writes NAME-rs.txt per
# board and rs-TAG-drv.txt (broker stop and start times) for rsan.py.
# env: PY (python with pyserial), MOSQUITTO (default mosquitto); the broker runs from this directory's mosq.conf.
D=$(cd "$(dirname "$0")" && pwd); S=$(cd "$D/../.." && pwd); G=${1:?tag}; shift
PY=${PY:?set PY}; MQ=${MOSQUITTO:-mosquitto}; DRV=$D/rs-$G-drv.txt; rm -f "$DRV"
names=(); caps=()
for b in "$@"; do
  n=${b%%:*}; p=${b#*:}; names+=("$n"); rm -f "$D/$n-rs.txt" "$D/$n-rs.err"
  "$PY" "$S/serial_capture.py" "$p" 250 "$D/$n-rs.txt" --reset --ms > /dev/null 2> "$D/$n-rs.err" &
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
fail() { echo "rs.sh: $1" >&2; kill $pub "${caps[@]}" 2> /dev/null; exit 1; }
sleep 35; pkill -f "[m]osquitto -c $D/mosq.conf" || fail "no broker running from $D/mosq.conf"
echo "$(date +%H:%M:%S).000 BROKER STOP" >> "$DRV"
sleep 10; (exec "$MQ" -c "$D/mosq.conf" >> "$D/mosq.log" 2>&1 < /dev/null) &
brk=$!; t=$(date +%H:%M:%S); sleep 1; kill -0 $brk 2> /dev/null || fail "broker did not start, see $D/mosq.log"
echo "$t.000 BROKER START" >> "$DRV"
sleep 209; kill $pub 2> /dev/null; bad=""
for k in "${!caps[@]}"; do wait "${caps[$k]}" || bad="$bad ${names[$k]}"; done
[ -z "$bad" ] || { echo "rs.sh: capture failed for$bad, see NAME-rs.err" >&2; exit 1; }
echo RSDONE
