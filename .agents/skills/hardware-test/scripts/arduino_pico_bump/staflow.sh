#!/bin/zsh
# staflow.sh NAME TREE PICO_SN BLE(0/1) : USB flash, then over Wi-Fi: API soak + page loads, SSE alone, OTA, reboot,
# short soak, and (BLE=1) a GATT hold through the proxy; serial captured the whole time (cappico22671.py, a serial_capture.py copy here)
# env: PY, PEER_MAC (the GATT server, for BLE=1)
D=${0:A:h}; N=$1; T=$2; SN=$3; BLE=$4
PY=${PY:?set PY}; PORTS=$D/../ports.py
export API_KEY=$(cat $D/apikey.txt)
pkill -f "[c]appico22671.py" 2>/dev/null; sleep 1
PORT=$($PY $PORTS --wait 2e8a --serial $SN --timeout 30) || { echo "no pico"; exit 1; }
$D/up.sh $N $T $PORT || exit 1
PORT=$($PY $PORTS --wait 2e8a --serial $SN --timeout 60) || { echo "pico gone"; exit 1; }
: > $D/$N-serial.log; ($PY $D/cappico22671.py $PORT 1100 $D/$N-serial.log 2> $D/$N-cap.err &)
H=""; for i in $(seq 1 30); do H=$(dscacheutil -q host -a name $N.local 2>/dev/null | awk '/ip_address/{print $2; exit}'); [ -n "$H" ] && ping -c1 -t2 $H >/dev/null && break; H=""; sleep 3; done
[ -n "$H" ] || { echo "no address"; exit 1; }; echo "$H" > $D/$N-host.txt; sleep 5
(for i in $(seq 1 36); do curl -s -o /dev/null -w "%{http_code}\n" -m 10 http://$H/ ; sleep 5; done > $D/$N-web.txt) &
echo "soak: $($PY -I $D/soak.py $H 180 $D/$N-soak.log)"; wait
echo "web: $(sort $D/$N-web.txt | uniq -c | tr '\n' ' ')"
timeout 30 curl -s -N http://$H/events > $D/$N-sse.txt; echo "sse 30s alone: states $(grep -c 'event: state' $D/$N-sse.txt) pings $(grep -c 'event: ping' $D/$N-sse.txt)"
$D/up.sh $N $T $H | tr '\n' ' '; echo; sleep 25
$PY -I $D/press.py $H restart; sleep 25
echo "after OTA+reboot: $($PY -I $D/soak.py $H 30 $D/$N-after.log)"
if [ $BLE = 1 ]; then echo "--- ble"; timeout 400 $PY -I $D/gatthold.py $H ${PEER_MAC:?set PEER_MAC} 180; echo "gatt rc=$?"; fi
sleep 3; pkill -f "[c]appico22671.py $PORT"; sleep 1
echo "serial: crash/assert lines $(grep -a -c -i -E 'CRASH|panic|HardFault|assert|lock not held' $D/$N-serial.log), boots $(grep -a -c 'Log initialized\|ESPHome version' $D/$N-serial.log), port losses $(grep -a -c 'port lost' $D/$N-serial.log), lines $(wc -l < $D/$N-serial.log)"
