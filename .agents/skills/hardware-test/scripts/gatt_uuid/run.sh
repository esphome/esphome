#!/bin/zsh
# run.sh NAME: build ../rp2_web_ble/picow.yaml as NAME, USB flash the Pico W proxy, list the GATT UUID server
# (server.yaml here) through it and compare with the ESP32 reference. Exit 0 only when the lists match.
# env: TREE, PY, PICO_SN, PEER_MAC (the server); esp32ref.json (gattlist.py through an ESP32 proxy) and apikey.txt here
D=${0:A:h}; N=$1; S=$D/..; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; C=$S/rp2_web_ble/picow.yaml
$S/pio_build.sh compile $N $C || exit 1
PORT=$($PY $S/ports.py --wait 2e8a --serial ${PICO_SN:?set PICO_SN} --timeout 30) || { echo "no pico"; exit 1; }
$S/pio_build.sh upload $N $C $PORT || exit 1
H=$($S/resolve.sh $N) || exit 1; sleep 5
rm -f $D/$N.json
API_KEY=$(cat $D/apikey.txt) timeout 600 $PY -I $D/gattlist.py $H ${PEER_MAC:?set PEER_MAC} $D/$N.json > $D/$N-list.txt
RC=$?; grep -v "^ *CHR 00002a\|DSC 00002902\|SVC 000018" $D/$N-list.txt
[ $RC = 0 ] && [ -s $D/$N.json ] || { echo "FAIL: gattlist.py exited $RC"; exit 1; }
$PY $D/cmp.py $D/esp32ref.json $D/$N.json
