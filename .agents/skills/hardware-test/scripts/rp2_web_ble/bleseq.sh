#!/bin/zsh
# bleseq.sh NAME: 180 s GATT hold through the Pico W proxy NAME (built by staflow.sh), then 4 short holds 10 s apart,
# with the serial log captured. PASS needs every hold to pass, no crash line and a capture that lasted the run.
# env: TREE, PY, PICO_SN, PEER_MAC (the ble_notify server); apikey.txt here
D=${0:A:h}; N=$1; S=$D/..; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; H=$(cat $D/$N-host.txt)
FAILS=0; fail() { echo "FAIL: $*"; FAILS=$((FAILS + 1)); }
export API_KEY=$(cat $D/apikey.txt)
PORT=$($PY $S/ports.py --wait 2e8a --serial ${PICO_SN:?set PICO_SN} --timeout 10) || { echo "FAIL: no pico"; exit 1; }
L=$D/$N-bleseq-serial.log
$PY $S/serial_capture.py $PORT 420 $L 2> $D/$N-bleseq-cap.err &
CAP=$!; trap 'kill $CAP 2>/dev/null' EXIT
sleep 2
timeout 300 $PY -I $D/gatthold.py $H ${PEER_MAC:?set PEER_MAC} 180 > $D/$N-hold.txt 2>&1 || fail "the 180 s hold"
grep -v '^uuids' $D/$N-hold.txt
for i in 1 2 3 4; do
  sleep 10
  timeout 90 $PY -I $D/gatthold.py $H $PEER_MAC 15 > $D/$N-hold$i.txt 2>&1 || fail "short hold $i"
  grep -E "PASS|FAIL|held" $D/$N-hold$i.txt | tr '\n' ' '; echo
done
sleep 3
kill -0 $CAP 2>/dev/null || fail "the capture ended before the run did (see $D/$N-bleseq-cap.err)"
kill $CAP; wait $CAP 2>/dev/null; trap - EXIT
CRASH=$(grep -a -c -i -E 'CRASH|panic|HardFault|assert' $L)
echo "serial: crash/assert $CRASH boots $(grep -a -c 'ESPHome version' $L) notify-response warnings $(grep -a -c 'Failed to send notify' $L) connects $(grep -a -c 'Connecting v3' $L)"
grep -a -E "Disconnected, reason|Connection failed|status" $L | cut -c1-120 | sort | uniq -c | sort -rn | head -5
[ "$CRASH" = 0 ] || fail "$CRASH crash lines"
[ $FAILS = 0 ] || exit 1
echo PASS
