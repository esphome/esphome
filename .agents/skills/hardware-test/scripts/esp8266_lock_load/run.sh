#!/bin/bash
# run.sh NAME SECONDS PPS: build NAME.yaml (next to this script, or a path) from TREE, flash it over serial, capture
# the log for SECONDS plus 160 (boot, up to 120 s for the IP, the load) and run wifi_load.py at PPS packets per second
# (0: no load). Prints the boot count, every reset reason and the last ALIVE line. Exit 0: a clean run (exactly one
# BOOT line); 1: the run finished and saw resets; 2: setup failed or the run is invalid (no load, capture cut short,
# or the sensor or IR loopback never worked).
# env: TREE (checkout under test), PORT, SC (path to serial_capture.py), PY (checkout python)
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; T=${2:?seconds}; P=${3:?pps}
W=${TREE:?set TREE}; PORT=${PORT:?set PORT}; SC=${SC:?set SC}; PY=${PY:-$W/venv/bin/python}
Y=$N; [ -f "$Y" ] || Y=$D/$N.yaml; Y=$(cd "$(dirname "$Y")" && pwd)/$(basename "$Y"); N=$(basename "$Y" .yaml)
O=$D/out-$N; mkdir -p "$O"
pkill -f "[s]erial_capture.py $PORT "
rm -f "$O/serial.txt" "$O/load.txt"
(cd "$W" && PYTHONPATH=$W $PY -m esphome compile "$Y") > "$O/compile.log" 2>&1 || { echo "$N compile failed"; exit 2; }
(cd "$W" && PYTHONPATH=$W $PY -m esphome upload "$Y" --device "$PORT") > "$O/upload.log" 2>&1 || { echo "$N upload failed"; exit 2; }
$PY "$SC" "$PORT" $((T + 160)) "$O/serial.txt" --reset > "$O/capture.log" 2>&1 &
CAP=$!
for _ in $(seq 60); do
  IP=$(LC_ALL=C grep -a -o -E "IP Address: [0-9.]+" "$O/serial.txt" | tail -1 | cut -d' ' -f3)
  [ -n "$IP" ] && break; sleep 2
done
if [ "$P" -gt 0 ]; then
  [ -n "$IP" ] || { kill "$CAP"; echo "$N no IP (check the secrets file)"; exit 2; }
  python3 "$D/wifi_load.py" "$IP" "$T" "$P" > "$O/load.txt" 2>&1 || { echo "$N INVALID: load failed (see $O/load.txt)"; BAD=1; }
else
  sleep "$T"
fi
wait "$CAP" || { echo "$N INVALID: capture failed (see $O/capture.log)"; BAD=1; }
BOOTS=$(LC_ALL=C grep -a -c 'BOOT reason' "$O/serial.txt")
echo "== $N pps=$P boots=$BOOTS"
LC_ALL=C grep -a "BOOT reason" "$O/serial.txt" | cut -c1-150
LC_ALL=C grep -a "ALIVE" "$O/serial.txt" | tail -1 | cut -c1-200
tail -1 "$O/load.txt" 2>/dev/null
ALIVE=$(LC_ALL=C grep -a "ALIVE" "$O/serial.txt" | tail -1)
# the workload must have run: a sensor answering searches, or decoded IR frames
case "$ALIVE" in
  *" ok=0 "*|*" rx_ok=0 "*|"") echo "$N INVALID: no ALIVE line or the workload did not run"; BAD=1 ;;
esac
# one-wire: the searches must find the sensor (busy_control runs no searches)
S=$(echo "$ALIVE" | sed -n 's/.* searches=\([0-9]*\) search_bad=\([0-9]*\).*/\1 \2/p')
if [ -n "$S" ]; then
  set -- $S
  [ "$1" -gt 0 ] && [ "$2" -ge "$1" ] && { echo "$N INVALID: no search found the sensor"; BAD=1; }
fi
# resets are a failure even when they also broke the load or the workload
[ "$BOOTS" -gt 1 ] && exit 1
[ -z "$BAD" ] && [ "$BOOTS" = 1 ] || exit 2
