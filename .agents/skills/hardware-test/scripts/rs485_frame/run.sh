#!/bin/bash
# run.sh CASES LABEL [SECONDS]: CASES is common, rsi2, rsx or rsd. Writes rs<CASES><LABEL>.yaml (own name, the case
# file as a package), builds it from TREE, flashes the ESP32-C3 over USB, captures SECONDS (default 40) and writes
# out-rs<CASES><LABEL>.log with the simulator (sim) and test (hwt) lines, time stamps and log line numbers stripped,
# so the logs of dev and the PR can be diffed. Fails when the capture has no DONE line or shows a crash.
# env: TREE (esphome checkout with rs485_frame), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); C=${1:?common, rsi2, rsx or rsd}; L=${2:?label}; S=${3:-40}
T=${TREE:?set TREE}; P=${PORT:?set PORT}; SC=${SC:?set SC}; PY=${PY:-$T/venv/bin/python}
[ -f "$R/$C.yaml" ] || { echo "no case file $C.yaml"; exit 1; }
NAME=rs$C$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
printf 'esphome:\n  name: %s\npackages:\n  cases: !include %s.yaml\n' "$NAME" "$C" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P "
(cd "$T" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/build.log" 2>&1) ||
  { echo "build or flash failed, see $OUT/build.log"; exit 1; }
$PY "$SC" "$P" "$S" "$OUT/serial.txt" --reset > /dev/null 2>&1 || { echo "serial capture failed"; exit 1; }
grep -a -E '\[(sim|hwt|rs485_frame)[]:]' "$OUT/serial.txt" | sed -E 's/^\[[0-9:.]*\]//; s/\[(sim|hwt|rs485_frame):[0-9]+\]/[\1]/' > "$R/out-$NAME.log"
echo "$NAME: $(grep -c '\[sim\]' "$R/out-$NAME.log") simulator lines, $(grep -c '\[hwt\]' "$R/out-$NAME.log") test lines"
CRASH=$(LC_ALL=C grep -a -c -E 'Guru|abort\(\)|Backtrace:|task_wdt: Task' "$OUT/serial.txt")
[ "$CRASH" = 0 ] || { echo "FAIL: $CRASH crash lines"; exit 1; }
grep -q DONE "$R/out-$NAME.log" || { echo "FAIL: no DONE (the case script did not finish)"; exit 1; }
echo PASS
