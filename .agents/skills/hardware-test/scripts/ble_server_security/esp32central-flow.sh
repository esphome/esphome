#!/bin/bash
# esp32central-flow.sh TAG: erase both boards, flash esp32central-srv.yaml (SERVER_PORT, from SERVER_TREE) and
# esp32central-cli.yaml (CLIENT_PORT, from TREE), then
#   A: capture a fresh pairing and three link cycles (170 s)
#   B: reset both together so each loads its bond from NVS, capture 130 s
# and count pairings, bond reuse and dropped bonds per phase. Logs go to out-ec<TAG>/. PASS when phase A read the MITM
# characteristic and phase B read it on the stored bond (no numeric comparison, no "Deleting unpaired device").
# env: TREE, SERVER_TREE (checkout with #17562, default TREE), SERVER_PORT, CLIENT_PORT, SERVER_BLE_MAC (from the
# server's esp32_ble dump), SC (path to serial_capture.py), PY (checkout python)
D=$(cd "$(dirname "$0")" && pwd); G=${1:?tag}; T=${TREE:?set TREE}; ST=${SERVER_TREE:-$T}; PY=${PY:-$T/venv/bin/python}
S=${SERVER_PORT:?set SERVER_PORT}; C=${CLIENT_PORT:?set CLIENT_PORT}; MAC=${SERVER_BLE_MAC:?set SERVER_BLE_MAC}; SC=${SC:?set SC}
O=$D/out-ec$G; rm -rf "$O"; mkdir -p "$O"
pkill -f "[s]erial_capture.py $S"; pkill -f "[s]erial_capture.py $C"
$PY -m esptool --port "$S" erase_flash > "$O/erase-srv.log" 2>&1 & E=$!
$PY -m esptool --port "$C" erase_flash > "$O/erase-cli.log" 2>&1 && wait $E || { echo "FAIL: erase"; exit 1; }
(cd "$ST" && $PY -m esphome -s name "ecsrv$G" run --no-logs "$D/esp32central-srv.yaml" --device "$S" > "$O/srv-upload.log" 2>&1) ||
  { echo "FAIL: server build or upload"; exit 1; }
(cd "$T" && $PY -m esphome -s name "eccli$G" -s server "$MAC" run --no-logs "$D/esp32central-cli.yaml" --device "$C" \
  > "$O/cli-upload.log" 2>&1) || { echo "FAIL: client build or upload"; exit 1; }
phase() {  # phase NAME SECONDS
  $PY "$SC" "$S" "$2" "$O/$1-srv.txt" --ms --reset > /dev/null 2>&1 & local cap=$!
  sleep 2
  $PY "$SC" "$C" "$(($2 - 2))" "$O/$1-cli.txt" --ms --reset > /dev/null 2>&1
  wait $cap
  [ -s "$O/$1-srv.txt" ] && [ -s "$O/$1-cli.txt" ] || { echo "FAIL: phase $1 capture empty"; exit 1; }
  local f=$O/$1-cli.txt
  echo "$G $1: connects $(grep -a -c 'CLI connected' "$f"), numeric comparisons $(grep -a -c 'CLI numeric comparison' "$f")," \
    "mitm reads $(grep -a -c 'CLI value mitm' "$f"), server writes $(grep -a -c 'SRV mitm write' "$O/$1-srv.txt")," \
    "unpaired deletes cli $(grep -a -c 'Deleting unpaired device' "$f") srv $(grep -a -c 'Deleting unpaired device' "$O/$1-srv.txt")"
  grep -a -h '\[bonds' "$O/$1-srv.txt" "$f" | sed 's/.*\]: //'
}
phase A 170
phase B 130
grep -a -q 'CLI value mitm' "$O/A-cli.txt" || { echo "FAIL: the client never read the MITM characteristic"; exit 1; }
grep -a -q 'CLI value mitm' "$O/B-cli.txt" || { echo "FAIL: phase B never read the MITM characteristic"; exit 1; }
NB=$(grep -a -c 'CLI numeric comparison' "$O/B-cli.txt"); UB=$(cat "$O/B-cli.txt" "$O/B-srv.txt" | grep -a -c 'Deleting unpaired device')
[ "$NB" = 0 ] && [ "$UB" = 0 ] || { echo "FAIL: phase B paired again ($NB numeric comparisons, $UB unpaired deletes)"; exit 1; }
echo PASS
