#!/bin/bash
# run.sh NAME SECONDS PPS: build NAME.yaml (next to this script, or a path) from TREE, flash it over serial, capture the
# log for SECONDS plus 40 and run wifi_load.py at PPS packets per second (0: no load). Prints the boot count, every
# reset reason and the last ALIVE line. A clean run has exactly one BOOT line.
# env: TREE (checkout under test), PORT, SC (path to serial_capture.py), PY (checkout python)
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; T=${2:?seconds}; P=${3:?pps}
W=${TREE:?set TREE}; PORT=${PORT:?set PORT}; SC=${SC:?set SC}; PY=${PY:-$W/venv/bin/python}
Y=$N; [ -f "$Y" ] || Y=$D/$N.yaml; Y=$(cd "$(dirname "$Y")" && pwd)/$(basename "$Y"); N=$(basename "$Y" .yaml)
O=$D/out-$N; mkdir -p "$O"
pkill -f "[s]erial_capture.py $PORT"
(cd "$W" && PYTHONPATH=$W $PY -m esphome compile "$Y") > "$O/compile.log" 2>&1 || { echo "$N compile failed"; exit 1; }
(cd "$W" && PYTHONPATH=$W $PY -m esphome upload "$Y" --device "$PORT") > "$O/upload.log" 2>&1 || { echo "$N upload failed"; exit 1; }
$PY "$SC" "$PORT" $((T + 40)) "$O/serial.txt" > "$O/capture.log" 2>&1 &
CAP=$!
for _ in $(seq 60); do
  IP=$(LC_ALL=C grep -a -o -E "IP Address: [0-9.]+" "$O/serial.txt" | tail -1 | cut -d' ' -f3)
  [ -n "$IP" ] && break; sleep 2
done
[ -z "$IP" ] && echo "$N no IP (check the secrets file)"
if [ "$P" -gt 0 ] && [ -n "$IP" ]; then python3 "$D/wifi_load.py" "$IP" "$T" "$P" > "$O/load.txt" 2>&1; else sleep "$T"; fi
wait "$CAP" || echo "$N capture failed (see $O/capture.log); counts cover part of the run"
echo "== $N pps=$P boots=$(LC_ALL=C grep -a -c 'BOOT reason' "$O/serial.txt")"
LC_ALL=C grep -a "BOOT reason" "$O/serial.txt" | cut -c1-150
LC_ALL=C grep -a "ALIVE" "$O/serial.txt" | tail -1 | cut -c1-200
tail -1 "$O/load.txt" 2>/dev/null
