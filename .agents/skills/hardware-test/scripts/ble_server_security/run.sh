#!/bin/bash
# usage: run.sh TAG IO AUTH   build and flash the server (serial) and the client (OTA), then pair once and reconnect twice
# Copy this directory into a scratch directory with a secrets.yaml (wifi_ssid, wifi_password) and run it there.
# IO/AUTH: none bond (Just Works) or display_yes_no sc_mitm_bond (numeric comparison, MITM).
# env: TREE (esphome checkout for the client), SERVER_TREE (checkout with esp32_ble_server security, #17562 or
# later; default TREE), SERVER_PORT (devkit serial port), CLIENT_HOST (client address), SERVER_BLE_MAC (from the
# server's esp32_ble dump), SC (path to serial_capture.py), IDF (default recommended), PY (default $TREE/venv/bin/python)
# A server keeps its bonds in NVS and refuses a weaker re-pairing: erase its flash before switching from nc to jw.
D=$(cd "$(dirname "$0")" && pwd); G=${1:?tag}; IO=${2:?io}; AUTH=${3:?auth}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
P=${SERVER_PORT:?set SERVER_PORT}; C=${CLIENT_HOST:?set CLIENT_HOST}; MAC=${SERVER_BLE_MAC:?set SERVER_BLE_MAC}; SC=${SC:?set SC}
E=(-s io "$IO" -s auth "$AUTH" -s idf "${IDF:-recommended}")
ST=${SERVER_TREE:-$T}
build() { local t=$1; shift; (cd "$t" && PYTHONUNBUFFERED=1 timeout 1800 $PY -m esphome "$@"); }
ok=
for i in 1 2 3; do
  build "$ST" -s name "bss-$G" "${E[@]}" run --no-logs "$D/server.yaml" --device "$P" > "$D/server-$G-upload.log" 2>&1 && ok=1 && break
  sleep 5
done
[ -n "$ok" ] || { echo "FAIL: server build or flash (does SERVER_TREE have #17562?)"; exit 1; }
build "$T" -s name "bsc-$G" "${E[@]}" -s server "$MAC" run --no-logs "$D/client.yaml" --device "$C" > "$D/client-$G-upload.log" 2>&1 ||
  { echo "FAIL: client build or upload"; exit 1; }
rm -f "$D/server-$G.txt"
$PY "$SC" "$P" 330 "$D/server-$G.txt" --reset > /dev/null 2>&1 & CAP=$!
trap 'kill $CAP 2>/dev/null' EXIT
sleep 25
R=("Disconnect" "wait:3" "Connect" "wait:10" "Pair" "wait:5" "Read secure" "Read mitm")
$PY "$D/drive.py" "$C" "$D/client-$G.txt" "wait:6" "Remove bond" "Disconnect" "wait:3" "Connect" "wait:10" \
  "Read secure" "Read mitm" "Write secure" "Subscribe" "wait:8" "Pair" "wait:6" "Read secure" "Read mitm" "Write secure" \
  "Subscribe" "wait:10" "${R[@]}" "${R[@]}" > /dev/null || { echo "FAIL: driver"; exit 1; }
wait $CAP; CAPRC=$?; trap - EXIT
[ "$CAPRC" = 0 ] && [ -s "$D/server-$G.txt" ] || { echo "FAIL: server capture failed or empty (rc $CAPRC)"; exit 1; }
MODE=jw; [ "$IO" = none ] || MODE=nc
$PY "$D/summary.py" "$MODE" "$D/client-$G.txt" "$D/server-$G.txt"
