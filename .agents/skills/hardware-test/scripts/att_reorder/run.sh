#!/bin/bash
# run.sh PROXY_YAML [ROUNDS]: build server.yaml and PROXY_YAML from TREE, flash both, capture the proxy and read the
# server's services through it ROUNDS times (default 3). Prints the services per round and the Bluedroid discovery
# errors. Exit 2 on a setup failure.
# env: TREE, SERVER_PORT, PROXY_PORT, SERVER_BLE_MAC (the server's BLE address), SC (path to serial_capture.py), PY
D=$(cd "$(dirname "$0")" && pwd); Y=$(cd "$(dirname "${1:?proxy yaml}")" && pwd)/$(basename "$1"); R=${2:-3}
W=${TREE:?set TREE}; SP=${SERVER_PORT:?set SERVER_PORT}; PP=${PROXY_PORT:?set PROXY_PORT}; MAC=${SERVER_BLE_MAC:?set SERVER_BLE_MAC}
SC=${SC:?set SC}; PY=${PY:-$W/venv/bin/python}; N=$(basename "$Y" .yaml); O=$D/out-$N; mkdir -p "$O"
(cd "$W" && PYTHONPATH=$W $PY -m esphome -s name attsrv run --no-logs "$D/server.yaml" --device "$SP") > "$O/server.log" 2>&1 ||
  { echo "server build or upload failed"; exit 2; }
(cd "$W" && PYTHONPATH=$W $PY -m esphome run --no-logs "$Y" --device "$PP") > "$O/proxy-upload.log" 2>&1 ||
  { echo "$N build or upload failed"; exit 2; }
rm -f "$O/proxy.txt"
$PY "$SC" "$PP" $((R * 15 + 90)) "$O/proxy.txt" --reset > "$O/capture.log" 2>&1 &
CAP=$!
IP=""
for _ in $(seq 40); do
  IP=$(LC_ALL=C grep -a -o -E "IP Address: [0-9.]+" "$O/proxy.txt" | tail -1 | cut -d' ' -f3)
  [ -n "$IP" ] && break; sleep 2
done
[ -n "$IP" ] || { kill "$CAP"; echo "$N no IP"; exit 2; }
sleep 5
timeout $((R * 30 + 60)) $PY "$D/gattget.py" "$IP" "$MAC" "$R" || { kill "$CAP"; echo "$N client failed"; exit 2; }
wait "$CAP"
echo "== $N"
LC_ALL=C grep -a -E "invalid handle|Service discovery complete|attswap swaps=[1-9]" "$O/proxy.txt" | sed 's/^\[[0-9:]*\]//' |
  sort | uniq -c
