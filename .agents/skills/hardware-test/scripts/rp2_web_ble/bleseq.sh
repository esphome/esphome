#!/bin/zsh
# bleseq.sh NAME: 180 s GATT hold through the Pico W proxy NAME (built by staflow.sh), then 4 short connects 10 s
# apart, with the serial log captured. Exits 1 when the hold fails or the log shows a crash. env: TREE, PY, PICO_SN, PEER_MAC (the ble_notify server); apikey.txt here
D=${0:A:h}; N=$1; S=$D/..; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; H=$(cat $D/$N-host.txt)
export API_KEY=$(cat $D/apikey.txt)
PORT=$($PY $S/ports.py --wait 2e8a --serial ${PICO_SN:?set PICO_SN} --timeout 10) || { echo "no pico"; exit 1; }
L=$D/$N-bleseq-serial.log
$PY $S/serial_capture.py $PORT 420 $L 2> $D/$N-bleseq-cap.err &
CAP=$!; trap 'kill $CAP 2>/dev/null' EXIT
sleep 2; timeout 300 $PY -I $D/gatthold.py $H ${PEER_MAC:?set PEER_MAC} 180 > $D/$N-hold.txt 2>&1; HOLD=$?
grep -v '^uuids' $D/$N-hold.txt
for i in 1 2 3 4; do sleep 10; timeout 90 $PY -I $D/gatthold.py $H $PEER_MAC 15 2>&1 | grep -E "PASS|FAIL|held" | tr '\n' ' '; echo; done
sleep 3; kill $CAP; wait $CAP 2>/dev/null; trap - EXIT
echo "serial: crash/assert $(grep -a -c -i -E 'CRASH|panic|HardFault|assert' $L) boots $(grep -a -c 'ESPHome version' $L) notify-response warnings $(grep -a -c 'Failed to send notify' $L) connects $(grep -a -c 'Connecting v3' $L)"
grep -a -E "Disconnected, reason|Connection failed|status" $L | cut -c1-120 | sort | uniq -c | sort -rn | head -5
CRASH=$(grep -a -c -i -E 'CRASH|panic|HardFault|assert' $L)
[ $HOLD = 0 ] && [ "$CRASH" = 0 ] || { echo "FAIL (hold exit $HOLD, crash lines $CRASH)"; exit 1; }
echo PASS
