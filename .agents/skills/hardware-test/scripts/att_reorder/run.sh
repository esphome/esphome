#!/bin/bash
# run.sh PROXY_YAML [ROUNDS]: build server.yaml and PROXY_YAML from TREE, flash both, capture the proxy and read the
# server's services through it ROUNDS times (default 3). Prints the services per round and the Bluedroid discovery
# errors. Exit 0: every round got all services; 1: a round got a partial table (the bug); 2: setup failed, a round
# did not connect, or the swap never fired (the result would be meaningless). PROXY_YAML must sit in this
# directory, next to attswap.hpp and components/.
# env: TREE, SERVER_YAML (default server.yaml), API_KEY (proxy api key), SERVER_PORT, PROXY_PORT, SERVER_BLE_MAC
# (the server's BLE address), SC (path to serial_capture.py), PY
D=$(cd "$(dirname "$0")" && pwd); Y=$(cd "$(dirname "${1:?proxy yaml}")" && pwd)/$(basename "$1"); R=${2:-3}
W=${TREE:?set TREE}; SP=${SERVER_PORT:?set SERVER_PORT}; PP=${PROXY_PORT:?set PROXY_PORT}; MAC=${SERVER_BLE_MAC:?set SERVER_BLE_MAC}
API_KEY=${API_KEY:?set API_KEY}; SC=${SC:?set SC}; PY=${PY:-$W/venv/bin/python}; N=$(basename "$Y" .yaml); O=$D/out-$N; mkdir -p "$O"
(cd "$W" && PYTHONPATH=$W $PY -m esphome -s name attsrv run --no-logs "$D/${SERVER_YAML:-server.yaml}" --device "$SP") > "$O/server.log" 2>&1 ||
  { echo "server build or upload failed"; exit 2; }
(cd "$W" && PYTHONPATH=$W $PY -m esphome run --no-logs "$Y" --device "$PP") > "$O/proxy-upload.log" 2>&1 ||
  { echo "$N build or upload failed"; exit 2; }
rm -f "$O/proxy.txt"
$PY "$SC" "$PP" $((R * 30 + 160)) "$O/proxy.txt" --reset > "$O/capture.log" 2>&1 &
CAP=$!
IP=""
for _ in $(seq 40); do
  IP=$(LC_ALL=C grep -a -o -E "IP Address: [0-9.]+" "$O/proxy.txt" | tail -1 | cut -d' ' -f3)
  [ -n "$IP" ] && break; sleep 2
done
[ -n "$IP" ] || { kill "$CAP"; echo "$N no IP"; exit 2; }
sleep 5
timeout $((R * 30 + 60)) $PY "$D/gattget.py" "$IP" "$MAC" "$R" | tee "$O/client.txt"
[ "${PIPESTATUS[0]}" = 0 ] || { kill "$CAP"; echo "$N client failed"; exit 2; }
wait "$CAP" || { echo "$N capture failed (see $O/capture.log)"; exit 2; }
echo "== $N"
LC_ALL=C grep -a -E "invalid handle|Service discovery complete|attswap swaps=[1-9]" "$O/proxy.txt" | sed 's/^\[[0-9:]*\]//' |
  sort | uniq -c
grep -a -q -E "attswap swaps=[1-9]" "$O/proxy.txt" || { echo "$N INVALID: the reorder never fired"; exit 2; }
# every round must list the reordered service (fff0) and the one after it (1826) with all their characteristics
grep -a -q "^R[0-9]* mtu=" "$O/client.txt" || { echo "$N no client output"; exit 2; }
grep -a "^R[0-9]* mtu=" "$O/client.txt" | grep -v -q "fff0(6) 1826(2)" && exit 1
exit 0
