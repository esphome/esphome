#!/bin/zsh
# apflow.sh NAME: Pico W fallback AP (picow-ap.yaml as NAME; use a fresh NAME per build) and an ESP32 client
# (client.yaml) that provisions it through /wifisave. Prints the AP client count changes, the provisioning and
# crash or reboot markers; exits 1 on a timeout, a crash line or a client that never got status 200. env: TREE, PY, PICO_SN, CLIENT_PORT, CLIENT_TREE (checkout for the client build)
D=${0:A:h}; N=$1; S=$D/..; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
SN=${PICO_SN:?set PICO_SN}; CPORT=${CLIENT_PORT:?set CLIENT_PORT}; CTREE=${CLIENT_TREE:?set CLIENT_TREE}
$S/pio_build.sh compile $N $D/picow-ap.yaml || exit 1
PORT=$($PY $S/ports.py --wait 2e8a --serial $SN --timeout 30) || { echo "no pico"; exit 1; }
$S/pio_build.sh upload $N $D/picow-ap.yaml $PORT || exit 1
PORT=$($PY $S/ports.py --wait 2e8a --serial $SN --timeout 60) || { echo "pico gone"; exit 1; }
$PY $S/serial_capture.py $PORT 360 $D/$N-serial.log 2> $D/$N-cap.err &
CAP=$!; trap 'kill $CAP $CCAP 2>/dev/null' EXIT
n=0; until grep -a -q "Starting fallback AP\|Captive portal" $D/$N-serial.log; do sleep 2; n=$((n+2)); [ $n -ge 150 ] && { echo "FAIL: the AP never came up"; exit 1; }; done
echo "AP up after ${n}s"
TREE=$CTREE $S/pio_build.sh compile $N-client $D/client.yaml && TREE=$CTREE $S/pio_build.sh upload $N-client $D/client.yaml $CPORT || exit 1
$PY $S/serial_capture.py $CPORT 200 $D/$N-client.log --reset 2> $D/$N-ccap.err &
CCAP=$!
n=0; until grep -a -q "real save status\|posting the real" $D/$N-client.log; do sleep 2; n=$((n+2)); [ $n -ge 180 ] && { echo "FAIL: the client never saved"; exit 1; }; done
echo "client posted after ${n}s; settling 60s"; sleep 60
kill $CAP $CCAP 2>/dev/null; wait 2>/dev/null; trap - EXIT
echo "--- DUT"
grep -a -E "AP clients|Starting fallback AP|Requested WiFi|Connected\$|IP Address|Disabling AP|CRASH|panic|HardFault|Log initialized|port lost" $D/$N-serial.log | sed -E 's/(IP Address: )[0-9.]+/\1<ip>/' | cut -c1-120 | head -40
echo "--- client"
grep -a -E "connected, ip|save status|posting" $D/$N-client.log | sed -E 's/ip [0-9.]+/ip <ip>/' | head
CRASH=$(grep -a -c -E "CRASH|panic|HardFault" $D/$N-serial.log)
grep -a -q "real save status 200" $D/$N-client.log && [ "$CRASH" = 0 ] || { echo "FAIL (crash lines $CRASH)"; exit 1; }
echo PASS
