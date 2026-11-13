#!/bin/zsh
# apflow.sh DUT TREE : Pico W fallback AP + ESP32 client provisioning through /wifisave; prints the AP client count
# changes, provisioning and crash/reboot markers. Captures with cappico22671.py (a serial_capture.py copy here).
# env: PY, PICO_SN (the Pico's USB serial number), CLIENT_PORT and CLIENT_TREE (the ESP32 client and its checkout)
D=${0:A:h}; N=$1; T=$2
PY=${PY:?set PY}; PORTS=$D/../ports.py
PICO_SN=${PICO_SN:?set PICO_SN}; CPORT=${CLIENT_PORT:?set CLIENT_PORT}; CTREE=${CLIENT_TREE:?set CLIENT_TREE}
pkill -f "[c]appico22671.py" ; sleep 1
PORT=$($PY $PORTS --wait 2e8a --serial $PICO_SN --timeout 30) || { echo "no pico"; exit 1; }
$D/up.sh $N $T $PORT || exit 1
PORT=$($PY $PORTS --wait 2e8a --serial $PICO_SN --timeout 60) || { echo "pico gone"; exit 1; }
: > $D/$N-serial.log; ($PY $D/cappico22671.py $PORT 360 $D/$N-serial.log 2> $D/$N-cap.err &)
n=0; until grep -a -q "Starting fallback AP\|Captive portal" $D/$N-serial.log; do sleep 2; n=$((n+2)); [ $n -ge 150 ] && { echo "no AP"; break; }; done
echo "AP up after ${n}s"
$D/up.sh c22671 $CTREE $CPORT
: > $D/$N-client.log; ($PY $D/cappico22671.py $CPORT 200 $D/$N-client.log --reset 2> $D/$N-ccap.err &)
n=0; until grep -a -q "real save status\|posting the real" $D/$N-client.log; do sleep 2; n=$((n+2)); [ $n -ge 180 ] && { echo "client never saved"; break; }; done
echo "client posted after ${n}s; settling 60s"; sleep 60
pkill -f "[c]appico22671.py"; sleep 1
echo "--- DUT"
grep -a -E "AP clients|Starting fallback AP|Requested WiFi|Connected\$|IP Address|Disabling AP|CRASH|panic|HardFault|Log initialized|port lost" $D/$N-serial.log | sed -E 's/(IP Address: )[0-9.]+/\1<ip>/' | cut -c1-120 | head -40
echo "--- client"
grep -a -E "connected, ip|save status|posting" $D/$N-client.log | sed -E 's/ip [0-9.]+/ip <ip>/' | head
