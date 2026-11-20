#!/bin/bash
# flow.sh NAME YAML MODE: esp32_ble_server retest on an ESP32-S3 (bles2.yaml, bles2b.yaml or bles2c.yaml). Output in
# out-NAME/.
# MODE meas: erase, flash, 60 s idle heap.
# MODE test: erase, flash, Improv over BLE (bad checksum first), 3 direct rounds, 3 hard resets with a round each, proxy
#   table, runtime create/stop/start/remove through the proxy and directly, then a create inside on_write after a
#   fresh boot.
# MODE grow (#22877): the same start as test, then a hard reset, 8 service creates inside on_write, the proxy table and
#   a round after the growth; REST=1 skips to the growth. dev crashes only on a create that reallocates the list.
# env: TREE, PORT (the S3's USB port), PY (with bleak and aioesphomeapi), PROXY (a bluetooth_proxy, optional),
#      API_KEY (for PROXY, else apikey.txt), SECRETS (wifi_ssid/wifi_password for Improv, default secrets.yaml here)
D=$(cd "$(dirname "$0")" && pwd); S=$D/..; cd "$D" || exit 1
N=$1 Y=$2 M=$3; P=${PORT:?set PORT}; PY=${PY:-python3}; SEC=${SECRETS:-$D/secrets.yaml}
[ -z "${API_KEY+x}" ] && [ -f apikey.txt ] && API_KEY=$(cat apikey.txt)
# bles2b.yaml and bles2c.yaml add a Battery service, so gattc.py rounds without it fail
BATTERY=0; [ "$Y" != bles2.yaml ] && BATTERY=1
export API_KEY PY TREE BATTERY
O=$D/out-$N; mkdir -p "$O"; C=$O/cap.txt
CAP=
cap() { $PY "$S/serial_capture.py" "$P" 3000 "$C" --append > /dev/null 2>> "$O/cap.err" & CAP=$!; }
stopcap() {
  [ -n "$CAP" ] && { kill "$CAP" 2>/dev/null || echo "CAPTURE DIED early, see $O/cap.err"; }
  wait "$CAP" 2>/dev/null; CAP=; sleep 1
}
reset() {
  stopcap
  if $PY -m esptool --port "$P" --after hard_reset chip_id > /dev/null 2>> "$O/reset.err"; then
    echo "REBOOT $1 $(date +%T)" >> "$O/reboots.txt"
  else
    echo "RESET FAILED $1 $(date +%T), see $O/reset.err" | tee -a "$O/reboots.txt"; exit 1
  fi
  cap; sleep 12
}
round() { $PY gattc.py "$N" "$1" "$2" > "$O/$2.jsonl" 2>&1; tail -1 "$O/$2.jsonl"; }
proxytable() {
  for t in 1 2 3; do
    echo "-- proxy table (try $t)"; timeout 180 $PY px.py "$PROXY" "$N" table > "$O/proxy.json" 2>&1
    grep -q '"mode": "table"' "$O/proxy.json" && break; sleep 5
  done
  tail -1 "$O/proxy.json" | cut -c1-600
}
echo "== $N $(git -C "$TREE" rev-parse --short=10 HEAD) $(date +%T) mode $M"
if [ "$M" != grow ] || [ "${REST:-0}" = 0 ]; then
  rm -f "$C"
  $PY -m esptool --port "$P" erase_flash > "$O/erase.log" 2>&1 || { echo "erase failed"; exit 1; }
  "$S/pio_build.sh" upload "$N" "$D/$Y" "$P" || exit 1
  cap
  if [ "$M" = meas ]; then
    sleep 75; stopcap
    grep -a "HEAP $N" "$C" | tail -8
    exit 0
  fi
  sleep 15
  echo "-- improv"; $PY improv2.py "$N" "$SEC" 2>&1 | tee "$O/improv.txt" | grep -E "SUMMARY|FAIL"
  sleep 10
  echo "-- 3 rounds"; round 3 rounds
  for i in 1 2 3; do
    reset $i; echo "-- reboot $i"; round 1 reboot$i
  done
fi
if [ "$M" = grow ]; then
  reset grow
  echo "-- grow 8 inside on_write"; $PY grow.py "$N" 8 > "$O/grow.json" 2>&1; cat "$O/grow.json"
  [ -n "$PROXY" ] && proxytable
  echo "-- round after grow"; round 1 aftergrow
else
  if [ -n "$PROXY" ]; then
    proxytable
    echo "-- runtime via proxy"; timeout 400 $PY px.py "$PROXY" "$N" rt 0,1,2,3,4 > "$O/rtproxy.json" 2>&1; tail -1 "$O/rtproxy.json"
  fi
  echo "-- runtime direct"; timeout 300 $PY rtmac.py "$N" 0,1,2,3,4 > "$O/rtmac.jsonl" 2>&1; cat "$O/rtmac.jsonl"
  reset side
  echo "-- side check after a fresh boot: create a service inside on_write"
  $PY grow.py "$N" 1 0x81 > "$O/side.json" 2>&1; cat "$O/side.json"
fi
sleep 8; stopcap
[ -s "$C" ] || { echo "CAPTURE EMPTY, counts below are not valid; see $O/cap.err"; exit 1; }
echo "-- device log counts"
for p in "WRITE on_write" "DESC on_write" "SRV on_connect" "SRV on_disconnect" "RT " "GROW" "MOVED" "BATTERY" \
  "Guru Meditation" "Backtrace" "\[E\]" "\[W\]"; do
  printf "%-28s %s\n" "$p" "$(grep -a -c -E "$p" "$C")"
done
grep -a -E "RT |GROW|BATTERY|Guru|Backtrace|EXCVADDR|\[E\]|\[W\]" "$C" | cut -c1-220 | head -30
