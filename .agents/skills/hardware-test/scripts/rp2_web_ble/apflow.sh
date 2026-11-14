#!/bin/zsh
# apflow.sh NAME: Pico W fallback AP (picow-ap.yaml as NAME; use a fresh NAME per build) and an ESP32 client
# (client.yaml) that provisions it through /wifisave. Prints the AP client count changes, the provisioning and crash
# or reboot markers. PASS needs a client on the AP, status 200 for the real save, the DUT on the station network
# with its AP disabled, no crash line and a capture that lasted the run; exits 1 otherwise.
# env: TREE, PY, PICO_SN, CLIENT_PORT, CLIENT_TREE (checkout for the client build)
D=${0:A:h}; N=$1; S=$D/..; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
SN=${PICO_SN:?set PICO_SN}; CPORT=${CLIENT_PORT:?set CLIENT_PORT}; CTREE=${CLIENT_TREE:?set CLIENT_TREE}
FAILS=0; fail() { echo "FAIL: $*"; FAILS=$((FAILS + 1)); }
# both builds first, so the DUT capture only has to cover the provisioning
$S/pio_build.sh compile $N $D/picow-ap.yaml || exit 1
TREE=$CTREE $S/pio_build.sh compile $N-client $D/client.yaml || exit 1
PORT=$($PY $S/ports.py --wait 2e8a --serial $SN --timeout 30) || { echo "FAIL: no pico"; exit 1; }
$S/pio_build.sh upload $N $D/picow-ap.yaml $PORT || exit 1
PORT=$($PY $S/ports.py --wait 2e8a --serial $SN --timeout 60) || { echo "FAIL: pico gone after the upload"; exit 1; }
L=$D/$N-serial.log; CL=$D/$N-client.log
$PY $S/serial_capture.py $PORT 600 $L 2> $D/$N-cap.err &
CAP=$!; trap 'kill $CAP $CCAP 2>/dev/null' EXIT
n=0; until grep -a -q "Starting fallback AP\|Captive portal" $L; do sleep 2; n=$((n+2)); [ $n -ge 150 ] && { echo "FAIL: the AP never came up"; exit 1; }; done
echo "AP up after ${n}s"
TREE=$CTREE $S/pio_build.sh upload $N-client $D/client.yaml $CPORT || exit 1
$PY $S/serial_capture.py $CPORT 200 $CL --reset 2> $D/$N-ccap.err &
CCAP=$!
n=0; until grep -a -q "real save status" $CL; do sleep 2; n=$((n+2)); [ $n -ge 180 ] && { echo "FAIL: the client never saved"; exit 1; }; done
echo "client posted after ${n}s; settling 60s"; sleep 60
kill -0 $CAP 2>/dev/null || fail "the DUT capture ended before the run did (see $D/$N-cap.err)"
kill $CAP $CCAP 2>/dev/null; wait 2>/dev/null; trap - EXIT
echo "--- DUT"
grep -a -E "AP clients|Starting fallback AP|Requested WiFi|Connected\$|IP Address|Disabling AP|CRASH|panic|HardFault|Log initialized|port lost" $L | sed -E 's/(IP Address: )[0-9.]+/\1<ip>/' | cut -c1-120 | head -40
echo "--- client"
grep -a -E "connected, ip|save status|posting" $CL | sed -E 's/ip [0-9.]+/ip <ip>/' | head
grep -a -q "AP clients [1-9]" $L || fail "the DUT never counted a client on its AP"
grep -a -q "real save status 200" $CL || fail "the real save did not return 200"
grep -a -q "Disabling AP" $L || fail "the DUT never disabled its AP"
# the fallback AP logs its own 192.168.4.x address too, so only another address shows the station side
grep -a "IP Address" $L | grep -v -q "192\.168\.4\." || fail "the DUT never got a station address"
CRASH=$(grep -a -c -E "CRASH|panic|HardFault" $L); [ "$CRASH" = 0 ] || fail "$CRASH crash lines"
[ $FAILS = 0 ] || exit 1
echo PASS
