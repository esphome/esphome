#!/bin/bash
# cross.sh PORT YAML NAME TREE_A TREE_B [CYCLES]: crossed A/B of two builds on one board, A B A B ... (default 2
# cycles). Per build: flash NAME-a or NAME-b over PORT, capture the serial log, ping 4 min idle, then the advert test
# (2 x 2 min) with a pinger. Back-to-back crossed runs keep a Wi-Fi that drifts over hours out of the comparison.
# env: PY, API_KEY (else apikey.txt)
PORT=$1 Y=$2 N=$3 TA=$4 TB=$5
D=$(cd "$(dirname "$0")" && pwd); cd "$D" || exit 1
[ -z "${API_KEY+x}" ] && [ -f apikey.txt ] && API_KEY=$(cat apikey.txt)
export API_KEY PYTHONUNBUFFERED=1
PY=${PY:-python3}
i=0
for _ in $(seq "${6:-2}"); do
  for W in a b; do
    i=$((i + 1)); T=x$i$W; TR=$TA; [ $W = b ] && TR=$TB
    TREE=$TR ../pio_build.sh upload "$N-$W" "$Y" "$PORT" > /dev/null || { echo "$T flash failed"; continue; }
    $PY ../serial_capture.py "$PORT" 700 "cap-$T.txt" --ms > /dev/null 2>&1 &
    CP=$!
    IP=$(../resolve.sh "$N-$W" 60) || { echo "$T did not resolve"; kill $CP; continue; }
    echo "$(date +%T) $T at $IP"
    sleep 20
    $PY ../ble_conn_interval/pinger.py "$IP" 240 "ping-$T-idle.txt"
    $PY ../ble_conn_interval/pinger.py "$IP" 300 "ping-$T-adv.txt" &
    PP=$!
    timeout 400 $PY drive.py "$IP" adv "$T-adv.jsonl" 120 > "$T-adv.err" 2>&1; echo "$(date +%T) $T adv rc=$?"
    wait $PP; kill $CP 2>/dev/null
  done
done
echo ALLDONE
