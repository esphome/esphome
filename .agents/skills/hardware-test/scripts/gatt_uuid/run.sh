#!/bin/zsh
# run.sh NAME TREE : USB flash the Pico W proxy, find it on Wi-Fi, list the S3 server through it, compare with the ESP32 reference
# env: PY, PICO_SN, PEER_MAC (the GATT server); esp32ref.json (gattlist.py through an ESP32 proxy) and apikey.txt in this dir
D=${0:A:h}; N=$1; T=$2; PY=${PY:?set PY}; PORTS=$D/../ports.py
PORT=$($PY $PORTS --wait 2e8a --serial ${PICO_SN:?set PICO_SN} --timeout 30) || { echo "no pico"; exit 1; }
$D/../arduino_pico_bump/up.sh $N $T $PORT || exit 1
H=""; for i in $(seq 1 30); do H=$(dscacheutil -q host -a name $N.local 2>/dev/null | awk '/ip_address/{print $2; exit}'); [ -n "$H" ] && ping -c1 -t2 $H >/dev/null && break; H=""; sleep 3; done
[ -n "$H" ] || { echo "no address"; exit 1; }; sleep 5
API_KEY=$(cat $D/apikey.txt) timeout 600 $PY -I $D/gattlist.py $H ${PEER_MAC:?set PEER_MAC} $D/$N.json | grep -v "^ *CHR 00002a\|DSC 00002902\|SVC 000018"
$PY $D/cmp.py $D/esp32ref.json $D/$N.json
