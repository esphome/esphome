#!/bin/bash
# run.sh LABEL: build https-dut.yaml as hs<LABEL> from TREE, flash it over PORT, read the address from the serial log,
# then run t1.sh, stall.py and t2.sh (t2.sh ends on a bad key, which leaves the listener off until a reset).
# Run it from a scratch copy of this directory with a secrets.yaml (wifi_ssid, wifi_password).
# env: TREE (checkout with web_server_idf HTTPS), PORT, SC (path to serial_capture.py), PY (checkout python)
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}
SC=${SC:?set SC}; O=$D/out-hs$L; rm -rf "$O"; mkdir -p "$O"
[ -f "$D/ext/httpstest/certs.h" ] || "$D/make_certs.sh" || exit 1
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome -s name "hs$L" run --no-logs "$D/https-dut.yaml" --device "$P" > "$O/upload.log" 2>&1) ||
  { echo "FAIL: build or upload"; exit 1; }
$PY "$SC" "$P" 240 "$O/serial.txt" --reset > /dev/null 2>&1 & CAP=$!; trap 'kill $CAP 2>/dev/null' EXIT
H=; for _ in $(seq 60); do
  H=$(grep -a -o -E 'IP Address: [0-9.]+' "$O/serial.txt" 2>/dev/null | tail -1 | awk '{print $3}'); [ -n "$H" ] && break; sleep 1
done
[ -n "$H" ] || { echo "FAIL: no IP address in the serial log"; exit 1; }
sleep 5; RC=0
"$D/t1.sh" "$H" | tee "$O/t1.txt"; [ "${PIPESTATUS[0]}" = 0 ] || RC=1
$PY "$D/stall.py" "$H" | tee "$O/stall.txt"; [ "${PIPESTATUS[0]}" = 0 ] || RC=1
"$D/t2.sh" "$H" | tee "$O/t2.txt"; [ "${PIPESTATUS[0]}" = 0 ] || RC=1
sleep 3
grep -a -E 'SWITCH to mode|https_server|httpd|Guru|abort\(\)|Backtrace|task_wdt' "$O/serial.txt" | cut -c1-160
grep -a -q -E 'Guru|abort\(\)|Backtrace|task_wdt' "$O/serial.txt" && { echo "FAIL: reset or watchdog"; RC=1; }
grep -a 'ALIVE' "$O/serial.txt" | sed -n '1p;$p'
[ "$RC" = 0 ] && echo PASS || { echo FAIL; exit 1; }
