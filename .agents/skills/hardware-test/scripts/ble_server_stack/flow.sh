#!/bin/bash
# flow.sh NAME YAML MODE: esp32_ble_server retest on an ESP32-S3 (bles2.yaml or bles2b.yaml). Output in out-NAME/.
# MODE meas: erase, flash, 60 s idle heap. MODE test: erase, flash, Improv over BLE (bad checksum first), 3 direct
# rounds, 3 hard resets with a round each, proxy table, runtime create/stop/start/remove through the proxy and
# directly, then a create inside on_write after a fresh boot.
# env: TREE, PORT (the S3's USB port), PY (with bleak and aioesphomeapi), PROXY (a bluetooth_proxy, optional),
#      API_KEY (for PROXY, else apikey.txt), SECRETS (wifi_ssid/wifi_password for Improv, default secrets.yaml here)
D=$(cd "$(dirname "$0")" && pwd); S=$D/..; cd "$D" || exit 1
N=$1 Y=$2 M=$3; P=${PORT:?set PORT}; PY=${PY:-python3}; SEC=${SECRETS:-$D/secrets.yaml}
[ -z "${API_KEY+x}" ] && [ -f apikey.txt ] && API_KEY=$(cat apikey.txt)
export API_KEY PY TREE
O=$D/out-$N; mkdir -p "$O"; C=$O/cap.txt
CAP=
cap() { $PY "$S/serial_capture.py" "$P" "$1" "$C" --append > /dev/null 2>&1 & CAP=$!; }
stopcap() { [ -n "$CAP" ] && kill "$CAP" 2>/dev/null; wait "$CAP" 2>/dev/null; CAP=; sleep 1; }
reset() { stopcap; $PY -m esptool --port "$P" --after hard_reset chip_id > /dev/null 2>&1; echo "REBOOT $1 $(date +%T)" >> "$O/reboots.txt"; cap 3000; sleep 12; }
echo "== $N $(git -C "$TREE" rev-parse --short=10 HEAD) $(date +%T) mode $M"
rm -f "$C"
$PY -m esptool --port "$P" erase_flash > "$O/erase.log" 2>&1 || { echo "erase failed"; exit 1; }
"$S/pio_build.sh" upload "$N" "$D/$Y" "$P" || exit 1
cap 3000
if [ "$M" = meas ]; then
  sleep 75; stopcap
  grep -a "HEAP $N" "$C" | tail -8
  exit 0
fi
sleep 15
echo "-- improv"; $PY improv2.py "$N" "$SEC" 2>&1 | tee "$O/improv.txt" | grep -E "SUMMARY|FAIL"
sleep 10
echo "-- 3 rounds"; $PY gattc.py "$N" 3 rounds > "$O/rounds.jsonl" 2>&1; tail -1 "$O/rounds.jsonl"
for i in 1 2 3; do
  reset $i
  echo "-- reboot $i"; $PY gattc.py "$N" 1 reboot$i > "$O/reboot$i.jsonl" 2>&1; tail -1 "$O/reboot$i.jsonl"
done
if [ -n "$PROXY" ]; then
  for t in 1 2 3; do
    echo "-- proxy table (try $t)"; timeout 180 $PY px.py "$PROXY" "$N" table > "$O/proxy.json" 2>&1; tail -1 "$O/proxy.json"
    grep -q '"mode": "table"' "$O/proxy.json" && break; sleep 5
  done
  echo "-- runtime via proxy"; timeout 400 $PY px.py "$PROXY" "$N" rt 0,1,2,3,4 > "$O/rtproxy.json" 2>&1; tail -1 "$O/rtproxy.json"
fi
echo "-- runtime direct"; timeout 300 $PY rtmac.py "$N" 0,1,2,3,4 > "$O/rtmac.jsonl" 2>&1; cat "$O/rtmac.jsonl"
reset side
echo "-- side check after a fresh boot: create a service inside on_write"
$PY - "$N" <<'PYEOF'
import asyncio, sys
from bleak import BleakClient, BleakScanner
async def main(name):
    dev = await BleakScanner.find_device_by_name(name, timeout=40)
    try:
        async with BleakClient(dev, timeout=30) as c:
            await c.write_gatt_char("5e5e00c0-0000-1000-8000-00805f9b34fb", bytes([0x81]), response=True)
            await asyncio.sleep(3)
        print("INCB write done")
    except Exception as e:
        print("INCB", type(e).__name__, e)
asyncio.run(main(sys.argv[1]))
PYEOF
sleep 8; stopcap
echo "-- device log counts"
for p in "WRITE on_write" "DESC on_write" "SRV on_connect" "SRV on_disconnect" "RT " "Guru Meditation" "\[E\]" "\[W\]"; do
  printf "%-28s %s\n" "$p" "$(grep -a -c -E "$p" "$C")"
done
grep -a -E "RT |\[E\]|\[W\]" "$C" | cut -c1-200 | head -30
