#!/bin/bash
# run.sh LABEL: build and flash the notify server (SERVER_PORT) and the ble_client (CLIENT_PORT) from TREE, capture
# the client 150 s (two link cycles) and count on_notify against on_value per sensor. Fails on a crash, no notify, the
# two triggers of a sensor firing a different number of times (the delayed txt one may miss the last notify), or no
# `services released` line (#21821).
# env: TREE (esphome checkout), SERVER_PORT, CLIENT_PORT, SERVER_BLE_MAC (from the server's esp32_ble dump; flash the
# server once and read it), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}
SP=${SERVER_PORT:?set SERVER_PORT}; CP=${CLIENT_PORT:?set CLIENT_PORT}; MAC=${SERVER_BLE_MAC:?set SERVER_BLE_MAC}
OUT=$R/out-bn$L; rm -rf "$OUT"; mkdir -p "$OUT"
pkill -f "[s]erial_capture.py $SP"; pkill -f "[s]erial_capture.py $CP"
(cd "$T" && $PY -m esphome -s name "bnsrv$L" run --no-logs "$R/server.yaml" --device "$SP" > "$OUT/server-upload.log" 2>&1) ||
  { echo "FAIL: server build or upload"; exit 1; }
(cd "$T" && $PY -m esphome -s name "bncli$L" -s server "$MAC" run --no-logs "$R/client.yaml" --device "$CP" \
  > "$OUT/client-upload.log" 2>&1) || { echo "FAIL: client build or upload"; exit 1; }
$PY "$SC" "$CP" 150 "$OUT/client.txt" --reset > /dev/null 2>&1 || { echo "FAIL: client capture got nothing"; exit 1; }
c() { grep -a -c "$1" "$OUT/client.txt"; }
echo "bn$L: connects $(c 'CLI connected'), num value $(c 'CLI num value') notify A $(c 'num notify A') B $(c 'num notify B')," \
  "txt value $(c 'CLI txt value') notify A $(c 'txt notify A') B $(c 'txt notify B'), services released $(c 'All clients established, services released')"
CRASH=$(sed -n '/CLI connected/,$p' "$OUT/client.txt" | grep -a -c -E 'Guru|abort\(\)|Backtrace|rst:0x|task_wdt')
[ "$CRASH" = 0 ] || { echo "FAIL: the client crashed or reset $CRASH times after connecting"; exit 1; }
[ "$(c 'num notify A')" -gt 0 ] && [ "$(c 'txt notify A')" -gt 0 ] || { echo "FAIL: no on_notify fired"; exit 1; }
[ "$(c 'txt notify B')" -gt 0 ] || { echo "FAIL: the delayed txt on_notify never fired"; exit 1; }
[ "$(c 'num notify A')" = "$(c 'num notify B')" ] || { echo "FAIL: the two num on_notify triggers fired a different number of times"; exit 1; }
# the 1 s delay can push the last txt notify B past the end of the capture
TD=$(($(c 'txt notify A') - $(c 'txt notify B')))
[ "$TD" -ge 0 ] && [ "$TD" -le 1 ] || { echo "FAIL: txt notify A and B differ by $TD"; exit 1; }
[ "$(c 'All clients established, services released')" -gt 0 ] || { echo "FAIL: services were never released (#21821)"; exit 1; }
echo PASS
