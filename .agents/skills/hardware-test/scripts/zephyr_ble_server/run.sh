#!/bin/bash
# usage: run.sh NAME [SECONDS]   flash NAME's UF2 (built from server.yaml) to the nRF52 board, then count connections
# for SECONDS (default 90) while the client from client.yaml loops connect and disconnect against it.
# Copy this directory into a scratch directory, build server.yaml there with -s name NAME, build and upload client.yaml
# once with -s server <address>, and run it there.
# env: NRF_SERIAL (the board's USB serial number, as ports.py prints it), TREE (esphome checkout for the client log),
# CLIENT_HOST (client address), SC (path to serial_capture.py), PORTS (path to ports.py),
# PY (default $TREE/venv/bin/python). Exits 0 PASS, 1 setup or capture failure, 2 no client, 3 FAIL.
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; T=${2:-90}; TR=${TREE:?set TREE}; PY=${PY:-$TR/venv/bin/python}
SER=${NRF_SERIAL:?set NRF_SERIAL}; C=${CLIENT_HOST:?set CLIENT_HOST}; SC=${SC:?set SC}; PO=${PORTS:?set PORTS}
UF2=$D/.esphome/build/$N/.pioenvs/$N/zephyr/zephyr.uf2
[ -f "$UF2" ] || { echo "FAIL: build $N first (no $UF2)"; exit 1; }
port() { $PY "$PO" | awk -v s="$SER" '$0 ~ s && !/239a:0051/ {print $1}' | head -1; }
VOL=$(ls -d /Volumes/*BOOT 2>/dev/null | head -1)
if [ -z "$VOL" ]; then
  # 1200 baud touch: only an app built with dfu: true answers it
  $PY -c "import serial,sys; s=serial.Serial(sys.argv[1], 1200); s.close()" "$(port)" 2>/dev/null
  for _ in $(seq 30); do VOL=$(ls -d /Volumes/*BOOT 2>/dev/null | head -1); [ -n "$VOL" ] && break; sleep 1; done
fi
[ -n "$VOL" ] || { echo "FAIL: no UF2 boot drive; double tap RESET on the board"; exit 1; }
# the drive may belong to another board: this board's app port must be gone while it sits in its bootloader
[ -z "$(port)" ] || { echo "FAIL: the board's app port is still up; is $VOL another board's bootloader?"; exit 1; }
sleep 1
cp "$UF2" "$VOL/" 2>/dev/null  # the copy reports an I/O error when the board reboots, which is normal
for _ in $(seq 30); do P=$(port); [ -n "$P" ] && break; sleep 1; done
[ -n "$P" ] || { echo "FAIL: the app's USB port did not come back"; exit 1; }
$PY "$SC" "$P" $((T + 10)) "$D/$N-server.txt" --ms > /dev/null 2>&1 & CAP=$!
(cd "$TR" && timeout "$T" env PYTHONPATH="$TR" $PY -m esphome logs "$D/client.yaml" --device "$C" \
  > "$D/$N-client.txt" 2>&1)
wait $CAP; CAPRC=$?
[ "$CAPRC" = 0 ] && [ -s "$D/$N-server.txt" ] || { echo "FAIL: server capture failed or empty (rc $CAPRC)"; exit 1; }
cnt() { LC_ALL=C grep -a -c "$1" "$2"; }
CC=$(cnt 'CONNECT [0-9]' "$D/$N-client.txt"); SV=$(cnt 'zephyr_ble_server.*Connected' "$D/$N-server.txt")
[ "$CC" -gt 0 ] || { echo "INVALID: the client log has no CONNECT line (client not running or not reachable)"; exit 2; }
F=$D/$N-server.txt
echo "$N: server connected $SV, disconnected $(cnt 'Disconnected from' "$F"), advertising started" \
  "$(cnt 'Advertising successfully' "$F"), advertising failed $(cnt 'Advertising failed' "$F"); client connects $CC"
# a leaked connection slot shows as one connection and nothing after it
if [ "$SV" -gt 1 ]; then echo PASS; else echo "FAIL: $SV server connections"; exit 3; fi
