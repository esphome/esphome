#!/bin/bash
# run.sh LABEL: build the automation trace config as at<LABEL> from TREE, flash it over PORT, read the device address
# from the serial log, record the marker trace with drive.py and keep the part from "DRIVER press Run All" on in
# out-at<LABEL>.trace. Diff the .trace files of dev and the PR; the boot run is partly missed while the API connects.
# Run it from a scratch copy of this directory with a secrets.yaml (wifi_ssid, wifi_password).
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python),
# BOARD (esp32 board, default esp32dev), VARIANT (default esp32), API_KEY (only if you add api encryption)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=at$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
cat > "$R/$NAME.yaml" <<YAML
esphome:
  name: $NAME
esp32:
  board: ${BOARD:-esp32dev}
  variant: ${VARIANT:-esp32}
  framework:
    type: esp-idf
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
packages:
  trace: !include automation-trace-common.yaml
YAML
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
$PY "$SC" "$P" 120 "$OUT/serial.txt" --reset > /dev/null 2>&1 &
CAP=$!; trap 'kill $CAP 2>/dev/null' EXIT
H=; for _ in $(seq 60); do
  H=$(grep -a -o -E 'IP Address: [0-9.]+' "$OUT/serial.txt" 2>/dev/null | tail -1 | awk '{print $3}'); [ -n "$H" ] && break; sleep 1
done
[ -n "$H" ] || { echo "FAIL: no IP address in the serial log"; exit 1; }
PYTHONPATH=$(dirname "$SC") $PY "$R/drive.py" "$H" "$OUT/markers.txt"; RC=$?
sed -n '/^DRIVER press Run All/,$p' "$OUT/markers.txt" > "$R/out-$NAME.trace"
CR=$(grep -a -c -E 'Guru|abort\(\)|Backtrace|task_wdt' "$OUT/serial.txt")
echo "$NAME: $(wc -l < "$R/out-$NAME.trace") trace lines, crashes $CR"
[ "$CR" = 0 ] || { echo "FAIL: a crash or watchdog in the serial log"; exit 1; }
[ "$RC" = 0 ] || { echo "FAIL: Run All never reached M done"; exit 1; }
grep -q -E "SHOULD_NOT_RUN|NOT_REACHED" "$R/out-$NAME.trace" && { echo "FAIL: a step that must be skipped ran"; exit 1; }
echo PASS
