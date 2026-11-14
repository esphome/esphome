#!/bin/zsh
# leakflow.sh NAME CONFIG HOST: upload CONFIG as NAME to DEVICE (default HOST, an OTA), then 3 rounds of 300 /events
# connects reset right after the GET and 1 round of 300 that read the response head first (neverconn.py), with the
# free heap before, right after and after a 30 s settle. PASS needs every settled heap within LEAK_TOL bytes (default
# 4096) of the idle heap; exits 1 otherwise. env: TREE, PY, DEVICE, LEAK_TOL
D=${0:A:h}; N=$1; C=$2; H=$3; S=$D/..; cd $D
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; TOL=${LEAK_TOL:-4096}; FAILS=0
heap() { curl -s -m 4 "http://$H/sensor/Heap%20free" | grep -o '"value":[0-9]*' | cut -d: -f2; }
$S/pio_build.sh upload $N $D/$C ${DEVICE:-$H} || exit 1
for _ in $(seq 1 40); do curl -s -m 3 -o /dev/null http://$H/ && break; sleep 3; done; sleep 15
IDLE=$(heap); echo "$N idle heap: $IDLE"; [ -n "$IDLE" ] || { echo "FAIL: no Heap free reading"; exit 1; }
for r in rst rst rst head; do
  hb=$(heap); res=$($PY -I $D/neverconn.py $H 300 $r); sleep 3; ha=$(heap); sleep 30; hs=$(heap)
  echo "$N $r: $res | before $hb | after $ha | settled $hs"
  [ -n "$hs" ] && [ $((IDLE - hs)) -le $TOL ] || { echo "FAIL: settled heap ${hs:-none} vs idle $IDLE"; FAILS=$((FAILS + 1)); }
done
[ $FAILS = 0 ] || exit 1
echo PASS
