#!/bin/zsh
# staflow.sh NAME CONFIG BLE(0/1): USB flash CONFIG (picow.yaml, pico2w.yaml) as NAME, then over Wi-Fi: API soak with
# page loads, SSE alone, OTA, restart, a short soak and (BLE=1) a GATT hold through the proxy. Serial captured
# throughout. PASS needs both soaks to pass, every page load to return 200, state events on the SSE stream, a reboot
# (lower Uptime over REST) after the OTA and after the restart, the GATT hold (BLE=1), no crash or assert line and a
# capture that lasted the run; exits 1 otherwise.
# env: TREE, PY, PICO_SN (the Pico's USB serial number), PEER_MAC (the ble_notify server, for BLE=1); apikey.txt here
D=${0:A:h}; N=$1; C=$2; BLE=${3:-0}; S=$D/..
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; SN=${PICO_SN:?set PICO_SN}
FAILS=0; fail() { echo "FAIL: $*"; FAILS=$((FAILS + 1)); }
export API_KEY=$(cat $D/apikey.txt)
$S/pio_build.sh compile $N $D/$C || exit 1
PORT=$($PY $S/ports.py --wait 2e8a --serial $SN --timeout 30) || { echo "FAIL: no pico"; exit 1; }
$S/pio_build.sh upload $N $D/$C $PORT || exit 1
PORT=$($PY $S/ports.py --wait 2e8a --serial $SN --timeout 60) || { echo "FAIL: pico gone after the upload"; exit 1; }
L=$D/$N-serial.log
$PY $S/serial_capture.py $PORT 1100 $L 2> $D/$N-cap.err &
CAP=$!; trap 'kill $CAP 2>/dev/null' EXIT
H=$($S/resolve.sh $N) || exit 1; echo "$H" > $D/$N-host.txt; sleep 5
(for i in $(seq 1 36); do curl -s -o /dev/null -w "%{http_code}\n" -m 10 http://$H/ ; sleep 5; done > $D/$N-web.txt) &
WEB=$!
$PY -I $D/soak.py $H 180 $D/$N-soak.log || fail "the 180 s API soak"
wait $WEB
echo "web: $(sort $D/$N-web.txt | uniq -c | tr '\n' ' ')"
[ "$(grep -c -v '^200$' $D/$N-web.txt)" = 0 ] && [ -s $D/$N-web.txt ] || fail "page loads other than 200"
timeout 30 curl -s -N http://$H/events > $D/$N-sse.txt
SSE=$(grep -c 'event: state' $D/$N-sse.txt); echo "sse 30s alone: states $SSE pings $(grep -c 'event: ping' $D/$N-sse.txt)"
[ "$SSE" -gt 0 ] || fail "no state events on the SSE stream"
# the Uptime sensor over REST (web_server entity URLs use the entity name); empty when there is no answer
upt() { curl -s -m 5 http://$H/sensor/Uptime | $PY -c 'import json, sys; print(int(json.load(sys.stdin)["value"]))' 2>/dev/null; }
# wait up to 120 s for the device to answer with an uptime lower than U0 (it rebooted)
rebooted() { local u; for _ in $(seq 1 60); do u=$(upt); [ -n "$1" ] && [ -n "$u" ] && [ "$u" -lt "$1" ] && return 0; sleep 2; done; return 1; }
U0=$(upt); [ -n "$U0" ] || fail "no Uptime reading over REST"
$S/pio_build.sh upload $N $D/$C $H || fail "the OTA"
rebooted $U0 || fail "no reboot seen within 120 s after the OTA"
U0=$(upt); $PY -I $S/api_button.py $H "$API_KEY" press Restart > /dev/null || fail "pressing Restart"
rebooted $U0 || fail "no reboot seen within 120 s after the restart"
$PY -I $D/soak.py $H 30 $D/$N-after.log || fail "the soak after OTA and restart"
if [ $BLE = 1 ]; then
  echo "--- ble"; timeout 400 $PY -I $D/gatthold.py $H ${PEER_MAC:?set PEER_MAC} 180 || fail "the GATT hold"
fi
sleep 3
kill -0 $CAP 2>/dev/null || fail "the capture ended before the run did (see $D/$N-cap.err)"
kill $CAP; wait $CAP 2>/dev/null; trap - EXIT
CRASH=$(grep -a -c -i -E 'CRASH|panic|HardFault|assert|lock not held' $L)
echo "serial: crash/assert lines $CRASH, boots $(grep -a -c 'Log initialized\|ESPHome version' $L), port losses $(grep -a -c 'port lost' $L), lines $(wc -l < $L)"
[ "$CRASH" = 0 ] || fail "$CRASH crash or assert lines"
[ $FAILS = 0 ] || exit 1
echo PASS
