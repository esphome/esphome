#!/bin/bash
# run.sh BUILD(pr|base) BOARD(32|66) : serial flash ho<BOARD><BUILD>, serve its own image, drive the cases
# Copy this directory to a scratch directory (secrets.yaml next to ho-common.yaml) and run it there.
# env: TREE (esphome checkout for BUILD), P32 or P66 (serial port of the board), SC (path to serial_capture.py),
# HOST_IP (this host's address as the device reaches it), PY (checkout python)
D=$(cd "$(dirname "$0")" && pwd); V=$1; B=$2; N=ho$B$V; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
SC=${SC:?set SC}; HOST=${HOST_IP:?set HOST_IP}
if [ "$B" = 32 ]; then P=${P32:?set P32}; else P=${P66:?set P66}; fi
OUT=$D/out-$V-$B; rm -rf "$OUT"; mkdir -p "$OUT"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome compile "$D/$V/$B/$N.yaml" > "$OUT/compile.log" 2>&1) || { echo "compile failed"; exit 1; }
(cd "$T" && $PY -m esphome upload "$D/$V/$B/$N.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "upload failed"; exit 1; }
# the exact image of this config, just compiled, never one found elsewhere
if [ "$B" = 32 ]; then IMG=$D/$V/$B/.esphome/build/$N/build/firmware.ota.bin
else IMG=$D/$V/$B/.esphome/build/$N/.pioenvs/$N/firmware.bin; fi
[ -f "$IMG" ] || { echo "no image for $N at $IMG"; exit 1; }
echo "image $IMG"
W=$OUT/www; mkdir -p "$W/auth"; cp "$IMG" "$W/fw.bin"; cp "$IMG" "$W/auth/fw.bin"
$PY -c "import hashlib,sys;print(hashlib.md5(open(sys.argv[1],'rb').read()).hexdigest())" "$IMG" > "$W/fw.md5"
cp "$W/fw.md5" "$W/auth/fw.md5"; echo 00000000000000000000000000000000 > "$W/bad.md5"
$PY "$SC" "$P" 400 "$OUT/serial.txt" --reset > /dev/null 2>&1 & CAP=$!
trap 'kill $CAP $SRV 2>/dev/null' EXIT
pkill -f "[f]wsrv.py"; $PY "$D/fwsrv.py" "$W" 8093 > "$OUT/server.txt" 2>&1 & SRV=$!
for i in $(seq 60); do IP=$(grep -a -o 'IP Address: [0-9.]*' "$OUT/serial.txt" | tail -1 | cut -d' ' -f3); [ -z "$IP" ] && IP=$(python3 -c "import socket;print(socket.gethostbyname('$N.local'))" 2>/dev/null); [ -n "$IP" ] && break; sleep 2; done
[ -n "$IP" ] || { echo "device not found"; exit 1; }
echo "device up"
$PY "$D/hodrive.py" "$IP" "http://$HOST:8093" "$W/fw.md5" > "$OUT/drive.txt" 2>&1; RC=$?
kill $SRV; pkill -f "[s]erial_capture.py $P"
grep -E 'RESULT|SUMMARY|never came back' "$OUT/drive.txt"; echo "server requests: $(grep -c . "$OUT/server.txt")"
exit $RC
