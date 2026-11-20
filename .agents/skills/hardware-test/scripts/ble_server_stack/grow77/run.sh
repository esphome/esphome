#!/bin/bash
# run.sh NAME YAML MODE(dev|pr): #22877 check on an ESP32-S3 (bles77.yaml or bles77b.yaml). Erase, flash, Improv,
# direct rounds (pr: 3 plus 2 hard resets), then a hard reset and 8 service creates inside on_write; pr also takes the
# proxy table and one round after the growth. dev crashes only on a create that reallocates the services list.
# env: as for ../flow.sh (TREE, PORT, PY, PROXY, API_KEY, SECRETS); REST=1 skips everything before the growth.
D=$(cd "$(dirname "$0")" && pwd); B=$D/..; S=$B/..; cd "$D" || exit 1
N=$1 Y=$2 M=$3; P=${PORT:?set PORT}; PY=${PY:-python3}; SEC=${SECRETS:-$B/secrets.yaml}
[ -z "${API_KEY+x}" ] && [ -f "$B/apikey.txt" ] && API_KEY=$(cat "$B/apikey.txt")
export API_KEY PY TREE
O=$D/out-$N; mkdir -p "$O"; C=$O/cap.txt
CAP=
cap() { $PY "$S/serial_capture.py" "$P" 3000 "$C" --append > /dev/null 2>&1 & CAP=$!; }
stopcap() { [ -n "$CAP" ] && kill "$CAP" 2>/dev/null; wait "$CAP" 2>/dev/null; CAP=; sleep 1; }
reset() { stopcap; $PY -m esptool --port "$P" --after hard_reset chip_id > /dev/null 2>&1; echo "REBOOT $1 $(date +%T)" >> "$O/reboots.txt"; cap; sleep 12; }
echo "== $N $(git -C "$TREE" rev-parse --short=10 HEAD) $(date +%T) $M"
if [ "${REST:-0}" = 0 ]; then
  rm -f "$C"
  $PY -m esptool --port "$P" erase_flash > "$O/erase.log" 2>&1 || { echo "erase failed"; exit 1; }
  "$S/pio_build.sh" upload "$N" "$D/$Y" "$P" || exit 1
  cap; sleep 15
  echo "-- improv"; $PY "$B/improv2.py" "$N" "$SEC" > "$O/improv.txt" 2>&1; grep -E "SUMMARY|FAIL|PASS" "$O/improv.txt" | tail -2
  sleep 8
  R=1; [ "$M" = pr ] && R=3
  echo "-- $R rounds"; $PY "$B/gattc.py" "$N" $R rounds > "$O/rounds.jsonl" 2>&1; tail -1 "$O/rounds.jsonl"
  if [ "$M" = pr ]; then
    for i in 1 2; do
      reset $i; echo "-- reboot $i"; $PY "$B/gattc.py" "$N" 1 reboot$i > "$O/reboot$i.jsonl" 2>&1; tail -1 "$O/reboot$i.jsonl"
    done
  fi
fi
reset grow
echo "-- grow 8 inside on_write"; $PY grow.py "$N" 8 > "$O/grow.json" 2>&1; cat "$O/grow.json"
if [ "$M" = pr ]; then
  if [ -n "$PROXY" ]; then
    echo "-- proxy table"
    for t in 1 2 3; do
      timeout 180 $PY "$B/px.py" "$PROXY" "$N" table > "$O/proxy.json" 2>&1; grep -q '"mode": "table"' "$O/proxy.json" && break; sleep 5
    done
    tail -1 "$O/proxy.json" | cut -c1-600
  fi
  echo "-- round after grow"; $PY "$B/gattc.py" "$N" 1 aftergrow > "$O/aftergrow.jsonl" 2>&1; tail -1 "$O/aftergrow.jsonl"
fi
sleep 5; stopcap
echo "-- counts"
for p in "GROW" "MOVED" "Guru Meditation" "Backtrace" "BATTERY" "WRITE on_write" "SRV on_connect" "BLE server setup successfully" "\[E\]"; do
  printf "%-30s %s\n" "$p" "$(grep -a -c -E "$p" "$C")"
done
grep -a -E "GROW|BATTERY|Guru|Backtrace|EXCVADDR|\[E\]" "$C" | cut -c1-220 | head -30
