#!/bin/zsh
# staflow.sh NAME CONFIG BLE(0/1): USB flash CONFIG (picow.yaml, pico2w.yaml) as NAME, then over Wi-Fi: API soak with
# page loads, SSE alone, OTA, restart, a short soak and (BLE=1) a GATT hold through the proxy. Serial captured throughout.
# Exits 1 when the device stops answering, the GATT hold fails or the serial log shows a crash or assert line.
# env: TREE, PY, PICO_SN (the Pico's USB serial number), PEER_MAC (the ble_notify server, for BLE=1); apikey.txt here
D=${0:A:h}; N=$1; C=$2; BLE=${3:-0}; S=$D/..
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; SN=${PICO_SN:?set PICO_SN}
export API_KEY=$(cat $D/apikey.txt)
$S/pio_build.sh compile $N $D/$C || exit 1
PORT=$($PY $S/ports.py --wait 2e8a --serial $SN --timeout 30) || { echo "no pico"; exit 1; }
$S/pio_build.sh upload $N $D/$C $PORT || exit 1
PORT=$($PY $S/ports.py --wait 2e8a --serial $SN --timeout 60) || { echo "pico gone"; exit 1; }
$PY $S/serial_capture.py $PORT 1100 $D/$N-serial.log 2> $D/$N-cap.err &
CAP=$!; trap 'kill $CAP 2>/dev/null' EXIT
H=$($S/resolve.sh $N) || exit 1; echo "$H" > $D/$N-host.txt; sleep 5
(for i in $(seq 1 36); do curl -s -o /dev/null -w "%{http_code}\n" -m 10 http://$H/ ; sleep 5; done > $D/$N-web.txt) &
WEB=$!
echo "soak: $($PY -I $D/soak.py $H 180 $D/$N-soak.log)"; wait $WEB
echo "web: $(sort $D/$N-web.txt | uniq -c | tr '\n' ' ')"
timeout 30 curl -s -N http://$H/events > $D/$N-sse.txt; echo "sse 30s alone: states $(grep -c 'event: state' $D/$N-sse.txt) pings $(grep -c 'event: ping' $D/$N-sse.txt)"
up() { for _ in $(seq 1 60); do curl -s -o /dev/null -m 3 http://$H/ && return 0; sleep 2; done; echo "no answer after 120 s"; return 1; }
$S/pio_build.sh upload $N $D/$C $H || exit 1; sleep 5; up || exit 1
$PY -I $S/api_button.py $H "$API_KEY" press Restart > /dev/null || exit 1; sleep 5; up || exit 1
echo "after OTA+reboot: $($PY -I $D/soak.py $H 30 $D/$N-after.log)"
GATT=0; if [ $BLE = 1 ]; then echo "--- ble"; timeout 400 $PY -I $D/gatthold.py $H ${PEER_MAC:?set PEER_MAC} 180; GATT=$?; echo "gatt rc=$GATT"; fi
sleep 3; kill $CAP; wait $CAP 2>/dev/null; trap - EXIT
echo "serial: crash/assert lines $(grep -a -c -i -E 'CRASH|panic|HardFault|assert|lock not held' $D/$N-serial.log), boots $(grep -a -c 'Log initialized\|ESPHome version' $D/$N-serial.log), port losses $(grep -a -c 'port lost' $D/$N-serial.log), lines $(wc -l < $D/$N-serial.log)"
CRASH=$(grep -a -c -i -E 'CRASH|panic|HardFault|assert|lock not held' $D/$N-serial.log)
[ $GATT = 0 ] && [ "$CRASH" = 0 ] || { echo "FAIL (gatt exit $GATT, crash lines $CRASH)"; exit 1; }
echo PASS
