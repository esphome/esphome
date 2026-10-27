#!/bin/bash
# usage: run.sh LABEL VARIANT   flash webbase.yaml, fetch /probe and /, then a web OTA of the same image
# Copy this directory into a scratch directory with a secrets.yaml (wifi_ssid, wifi_password) and run it there.
# VARIANT is ws, cp or prom (see webbase.yaml). The cp variant presses "Start portal" over the API first.
# env: TREE (esphome checkout), PORT (board serial port), PLAT (esp32 or esp8266), IP (the board's address),
# SC (path to serial_capture.py), PY (default $TREE/venv/bin/python)
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; V=${2:?variant}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
P=${PORT:?set PORT}; IP=${IP:?set IP}; SC=${SC:?set SC}; PL=${PLAT:-esp32}; N=web-$V-$PL-$L; C=$D/$N-serial.txt
S=$(cd "$D/.." && pwd)
if [ "$PL" = esp32 ]; then BIN=$D/.esphome/build/$N/build/$N.bin; else BIN=$D/.esphome/build/$N/.pioenvs/$N/firmware.bin; fi
(cd "$T" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome -s name "$N" -s plat "$PL" -s variant "$V" \
  upload "$D/webbase.yaml" --device "$P" > "$D/$N-upload.log" 2>&1) || { echo "FAIL: build or upload"; exit 1; }
$PY "$SC" "$P" 150 "$C" --reset > /dev/null 2>&1 & CAP=$!
up() { for _ in $(seq 1 40); do ping -c1 -t1 "$IP" > /dev/null && return 0; sleep 2; done; return 1; }
up || { echo "FAIL: board not reachable"; kill $CAP; exit 1; }; sleep 8
[ "$V" = cp ] && { $PY "$S/api_button.py" "$IP" - press "Start portal"; sleep 2; }
PROBE=$(curl -s -m 10 -w ' %{http_code}' "http://$IP/probe")
echo "GET /probe: $PROBE"
echo "GET /: $(curl -s -m 10 -o /dev/null -w '%{http_code} %{size_download} B' "http://$IP/")"
[ "$V" = prom ] && echo "GET /metrics: $(curl -s -m 10 -o /dev/null -w '%{http_code} %{size_download} B' "http://$IP/metrics")"
OTA=$(curl -s -m 120 -o /dev/null -F "update=@$BIN" -w '%{http_code}' "http://$IP/update")
echo "POST /update ($(wc -c < "$BIN" | tr -d ' ') B): http=$OTA"
sleep 5; up; sleep 6
$PY "$S/ota_timing/apiinfo.py" "$IP=$D/.esphome/build/$N/build_info.json" 2>&1 | tail -1; API=${PIPESTATUS[0]}
wait $CAP
fail=0
[ "$PROBE" = "probe-ok 200" ] || { echo "FAIL: /probe"; fail=1; }
[ "$OTA" = 200 ] || { echo "FAIL: web OTA"; fail=1; }
[ "$API" = 0 ] || { echo "FAIL: board not back on the uploaded build after the web OTA"; fail=1; }
grep -a -q -E "Guru|abort\(\)|panic|Exception \(" "$C" && { echo "FAIL: crash"; fail=1; }
[ $fail = 0 ] && echo PASS; exit $fail
