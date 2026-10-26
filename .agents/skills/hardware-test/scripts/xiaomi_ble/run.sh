#!/bin/bash
# usage: run.sh inject|ota pr|base   build and flash the receiver, capture its serial log, compare the counts
# Copy this directory into a scratch directory with a secrets.yaml (wifi_ssid, wifi_password, api_key) and run it there.
# inject: xbinject.yaml injects the frames into its own tracker. ota: xbrecv.yaml listens to xbadv.yaml, which must
# already run on a second ESP32 (python gen.py --ota, then flash xbadv.yaml once).
# env: TREE (esphome checkout to build from), PORT (receiver serial port), SC (path to serial_capture.py),
# PY (checkout python), ADV (advertiser address, ota only)
D=$(cd "$(dirname "$0")" && pwd); MODE=${1:?inject or ota}; B=${2:?pr or base}; T=${TREE:?set TREE}
P=${PORT:?set PORT}; SC=${SC:?set SC}; PY=${PY:-$T/venv/bin/python}
cd "$D" || exit 1
if [ "$MODE" = ota ]; then
  A=${ADV:?set ADV}; Y=xbrecv; SECS=130; LOSS=2
  $PY gen.py --ota > gen.txt || { echo "gen.py failed"; exit 1; }
else
  Y=xbinject; SECS=80; LOSS=0
  $PY gen.py > gen.txt || { echo "gen.py failed"; exit 1; }
fi
OUT=$D/$MODE-$B; mkdir -p "$OUT"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome run --no-logs "$D/$Y.yaml" --device "$P" > "$OUT/upload.log" 2>&1) \
  || { echo "FAIL: build or upload failed, see $OUT/upload.log"; exit 1; }
$PY "$SC" "$P" $SECS "$OUT/cap.txt" --reset > /dev/null 2>&1 & CAP=$!
trap 'kill $CAP 2>/dev/null' EXIT
if [ "$MODE" = ota ]; then
  # 18 s lets the receiver boot and start scanning, so the counter 0 boot frame is the first one it hears
  sleep 18
  CODE=$(curl -s -m 5 -X POST -d "" -o /dev/null -w "%{http_code}" "http://$A/button/start/press")
  [ "$CODE" = 200 ] || { echo "FAIL: advertiser start button returned $CODE"; exit 1; }
fi
wait $CAP; trap - EXIT
[ -s "$OUT/cap.txt" ] || { echo "FAIL: serial capture got nothing"; exit 1; }
CRASH=$(grep -acE 'Guru|abort\(\)|Backtrace:' "$OUT/cap.txt")
grep -a "\[probe" "$OUT/cap.txt" | tail -3
$PY count.py "$OUT/cap.txt" "$B" --allow-loss $LOSS; RC=$?
echo "crash lines: $CRASH"
[ "$RC" = 0 ] && [ "$CRASH" = 0 ]
