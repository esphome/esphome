#!/bin/bash
# run.sh LABEL: build the channel DUT as wc<LABEL> from TREE, flash it over PORT and capture 420 s while the bench AP
# (bench-ap.yaml, already flashed on AP_PORT) is taken down at 150 s (esptool leaves it in the bootloader) and
# brought back at 270 s. Prints every ssid and channel change. Run it from a scratch copy of this directory with a
# secrets.yaml (wifi_ssid, wifi_password, bench_ssid, bench_password).
# env: TREE, PORT, AP_PORT, SC (path to serial_capture.py), PY (checkout python), BOARD (default esp32dev),
# VARIANT (default esp32)
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}; A=${AP_PORT:?set AP_PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=wc$L; O=$D/out-$NAME; rm -rf "$O"; mkdir -p "$O"
printf 'esphome:\n  name: %s\nesp32:\n  board: %s\n  variant: %s\n  framework:\n    type: esp-idf\npackages:\n  dut: !include dut-common.yaml\n' \
  "$NAME" "${BOARD:-esp32dev}" "${VARIANT:-esp32}" > "$D/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$D/$NAME.yaml" --device "$P" > "$O/upload.log" 2>&1) || { echo "FAIL: build or upload"; exit 1; }
$PY "$SC" "$P" 420 "$O/serial.txt" --reset > /dev/null 2>&1 & CAP=$!
sleep 150; echo "$(date +%T) AP DOWN" >> "$O/driver.txt"; $PY -m esptool --port "$A" --after no-reset chip_id > /dev/null 2>&1
sleep 120; echo "$(date +%T) AP UP" >> "$O/driver.txt"; $PY -m esptool --port "$A" --after hard-reset chip_id > /dev/null 2>&1
wait $CAP
cat "$O/driver.txt"
grep -a -E 'EV (ssid|channel|wifi)|Beacon Timeout|Connecting to' "$O/serial.txt" | sed 's/\x1b\[[0-9;]*m//g' | cut -c1-150
grep -a -q "EV channel" "$O/serial.txt" || { echo "FAIL: no channel sensor update"; exit 1; }
