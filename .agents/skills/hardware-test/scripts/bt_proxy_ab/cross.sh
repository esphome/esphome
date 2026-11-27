#!/bin/bash
# cross.sh PORT YAML NAME TREE_A TREE_B [CYCLES]: crossed A/B of two builds on one board, A B A B ... (default 2
# cycles). Per build: flash NAME-a or NAME-b over PORT, capture the serial log, ping 4 min idle, then the advert test
# (2 x 2 min) with a pinger. Back-to-back crossed runs keep a Wi-Fi that drifts over hours out of the comparison.
# Exits 1 when a flash, a lookup or an advert run failed. env: PY, API_KEY (else apikey.txt)
PORT=$1 Y=$2 N=$3 TA=$4 TB=$5
D=$(cd "$(dirname "$0")" && pwd); cd "$D" || exit 1
[ -z "${API_KEY+x}" ] && [ -f apikey.txt ] && API_KEY=$(cat apikey.txt)
export API_KEY PYTHONUNBUFFERED=1
PY=${PY:-python3}
# drive.py and pinger.py append, so results of an earlier crossed run here would mix into this one
if compgen -G "x*-adv.jsonl" > /dev/null || compgen -G "ping-x[0-9]*-*.txt" > /dev/null; then
  echo "x*-adv.jsonl or ping-x*.txt files from an earlier run exist here; move them away" >&2; exit 2
fi
i=0 fail=0 CP= PP=
# stop the capture and pinger on any exit, so the port is free for the next flash
trap 'kill $CP $PP 2> /dev/null' EXIT
trap 'exit 130' INT TERM
for _ in $(seq "${6:-2}"); do
  for W in a b; do
    i=$((i + 1)); T=x$i$W; TR=$TA; [ $W = b ] && TR=$TB
    TREE=$TR ../pio_build.sh upload "$N-$W" "$Y" "$PORT" > "flash-$T.log" 2>&1 ||
      { echo "$T flash failed, see flash-$T.log"; fail=1; continue; }
    $PY ../serial_capture.py "$PORT" 700 "cap-$T.txt" --ms > /dev/null 2> "cap-$T.err" &
    CP=$!
    sleep 2; kill -0 $CP 2> /dev/null || { echo "$T capture did not start, see cap-$T.err"; fail=1; continue; }
    IP=$(../resolve.sh "$N-$W" 60) || { echo "$T did not resolve"; fail=1; kill $CP; continue; }
    echo "$(date +%T) $T at $IP"
    sleep 20
    $PY ../ble_conn_interval/pinger.py "$IP" 240 "ping-$T-idle.txt" || { echo "$T idle pinger failed"; fail=1; }
    $PY ../ble_conn_interval/pinger.py "$IP" 300 "ping-$T-adv.txt" &
    PP=$!
    timeout 400 $PY drive.py "$IP" adv "$T-adv.jsonl" 120 > "$T-adv.err" 2>&1; rc=$?
    echo "$(date +%T) $T adv rc=$rc"; [ $rc = 0 ] || fail=1
    wait $PP || { echo "$T advert pinger failed"; fail=1; }
    kill -0 $CP 2> /dev/null || { echo "$T capture ended early, see cap-$T.err"; fail=1; }
    kill $CP 2>/dev/null
  done
done
echo ALLDONE fail=$fail
exit $fail
