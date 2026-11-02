#!/bin/bash
# usage: run.sh LABEL VARIANT   flash webbase.yaml, fetch /probe and /, then a web OTA of the same image
# Copy this directory into a scratch directory with a secrets.yaml (wifi_ssid, wifi_password) and run it there.
# VARIANT is ws, cp or prom (see webbase.yaml). The cp variant presses "Start portal" over the API first.
# env: TREE (esphome checkout), PORT (board serial port), PLAT (esp32 or esp8266), IP (the board's address),
# SC (path to serial_capture.py), PY (default $TREE/venv/bin/python), SKILL_SCRIPTS (the skill's scripts directory)
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; V=${2:?variant}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
P=${PORT:?set PORT}; IP=${IP:?set IP}; SC=${SC:?set SC}; PL=${PLAT:-esp32}; N=web-$V-$PL-$L; C=$D/$N-serial.txt
# api_button.py and ota_timing/apiinfo.py live in the skill's scripts directory; set SKILL_SCRIPTS when this
# directory was copied elsewhere.
S=${SKILL_SCRIPTS:-$(cd "$D/.." && pwd)}
[ -f "$S/api_button.py" ] && [ -f "$S/ota_timing/apiinfo.py" ] || { echo "FAIL: set SKILL_SCRIPTS to the skill's scripts directory"; exit 1; }
if [ "$PL" = esp32 ]; then BIN=$D/.esphome/build/$N/build/$N.bin; else BIN=$D/.esphome/build/$N/.pioenvs/$N/firmware.bin; fi
(cd "$T" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome -s name "$N" -s plat "$PL" -s variant "$V" \
  run --no-logs "$D/webbase.yaml" --device "$P" > "$D/$N-upload.log" 2>&1) || { echo "FAIL: build or upload"; exit 1; }
rm -f "$C"
# Longer than the worst case below: 80 s to come up, 120 s of OTA, 80 s to come back.
$PY "$SC" "$P" 300 "$C" --reset > /dev/null 2>&1 & CAP=$!
# Wait on the API port: in the cp variant the HTTP server only starts when the portal does, also after the OTA reboot.
up() {
  for _ in $(seq 1 40); do
    $PY -c 'import socket, sys; socket.create_connection((sys.argv[1], 6053), 1).close()' "$IP" 2> /dev/null && return 0
    sleep 2
  done
  return 1
}
up || { echo "FAIL: board not reachable"; kill $CAP; exit 1; }; sleep 8
fail=0
if [ "$V" = cp ]; then
  $PY "$S/api_button.py" "$IP" - press "Start portal" || { echo "FAIL: Start portal press"; fail=1; }
  sleep 2
  # The captive portal only answers /config.json while it is active.
  CFG=$(curl -s -m 10 -o /dev/null -w '%{http_code}' "http://$IP/config.json")
  echo "GET /config.json: $CFG"; [ "$CFG" = 200 ] || { echo "FAIL: captive portal not active"; fail=1; }
fi
PROBE=$(curl -s -m 10 -w ' %{http_code}' "http://$IP/probe")
echo "GET /probe: $PROBE"
ROOT=$(curl -s -m 10 -o /dev/null -w '%{http_code}' "http://$IP/")
echo "GET /: $ROOT"
# prometheus has no page at / (no answer on ESP32, 404 on ESP8266, on dev too)
[ "$V" = prom ] || [ "$ROOT" = 200 ] || { echo "FAIL: GET /"; fail=1; }
if [ "$V" = prom ]; then
  MET=$(curl -s -m 10 -o /dev/null -w '%{http_code}' "http://$IP/metrics")
  echo "GET /metrics: $MET"; [ "$MET" = 200 ] || { echo "FAIL: /metrics"; fail=1; }
fi
# A failed update also answers 200, with "Update Failed!" in the body.
OTA=$(curl -s -m 120 -F "update=@$BIN" -w ' %{http_code}' "http://$IP/update" | tr -d '\r\n')
echo "POST /update ($(wc -c < "$BIN" | tr -d ' ') B): $OTA"
sleep 5
if up; then
  sleep 6
  $PY "$S/ota_timing/apiinfo.py" "$IP=$D/.esphome/build/$N/build_info.json" 2>&1 | tail -1; API=${PIPESTATUS[0]}
else
  echo "FAIL: board not back after the web OTA"; fail=1; API=1
fi
wait $CAP; CAPRC=$?
[ "$CAPRC" = 0 ] && [ -s "$C" ] || { echo "FAIL: serial capture failed or empty (rc $CAPRC)"; fail=1; }
[ "$PROBE" = "probe-ok 200" ] || { echo "FAIL: /probe"; fail=1; }
case "$OTA" in *"Update Successful!"*" 200") ;; *) echo "FAIL: web OTA"; fail=1;; esac
[ "$API" = 0 ] || { echo "FAIL: board not on the uploaded build after the web OTA"; fail=1; }
# The image is the same build, so prove the reboot from the log: setup finishes again after the OTA reboot line.
grep -a -q "Performing OTA reboot now" "$C" || { echo "FAIL: no OTA reboot in the serial log"; fail=1; }
sed -n '/Performing OTA reboot now/,$p' "$C" | grep -a -q 'setup() finished' || { echo "FAIL: no boot after the OTA"; fail=1; }
grep -a -q -E "Guru|abort\(\)|panic|[eE]xception \(|Soft WDT reset|wdt reset" "$C" && { echo "FAIL: crash"; fail=1; }
[ $fail = 0 ] && echo PASS; exit $fail
