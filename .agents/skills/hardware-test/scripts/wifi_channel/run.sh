#!/bin/bash
# run.sh LABEL: build the channel DUT as wc<LABEL> from TREE, flash it over PORT and capture 420 s while the bench AP
# (bench-ap.yaml, already flashed on AP_PORT) is taken down at 150 s (esptool leaves it in the bootloader) and
# brought back at 270 s. Prints every ssid and channel change, and PASS when the connected channel started and ended on
# BENCH_CHANNEL and was on another channel in between, with no reset; FAIL otherwise. Run it from a scratch copy of this directory with a
# secrets.yaml (wifi_ssid, wifi_password, bench_ssid, bench_password).
# env: TREE, PORT, AP_PORT, SC (path to serial_capture.py), PY (checkout python), BOARD (default esp32dev),
# VARIANT (default esp32), BENCH_CHANNEL (the bench-ap.yaml channel, default 11)
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}; A=${AP_PORT:?set AP_PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; BC=${BENCH_CHANNEL:-11}; NAME=wc$L; O=$D/out-$NAME; rm -rf "$O"; mkdir -p "$O"
printf 'esphome:\n  name: %s\nesp32:\n  board: %s\n  variant: %s\n  framework:\n    type: esp-idf\npackages:\n  dut: !include dut-common.yaml\n' \
  "$NAME" "${BOARD:-esp32dev}" "${VARIANT:-esp32}" > "$D/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$D/$NAME.yaml" --device "$P" > "$O/upload.log" 2>&1) || { echo "FAIL: build or upload"; exit 1; }
$PY "$SC" "$P" 420 "$O/serial.txt" --reset > /dev/null 2>&1 & CAP=$!
sleep 150; echo "$(date +%T) AP DOWN" >> "$O/driver.txt"; $PY -m esptool --port "$A" --after no-reset chip_id > "$O/ap-down.log" 2>&1 ||
  echo "esptool could not take the AP down, see ap-down.log" >> "$O/driver.txt"
sleep 120; echo "$(date +%T) AP UP" >> "$O/driver.txt"; $PY -m esptool --port "$A" --after hard-reset chip_id > "$O/ap-up.log" 2>&1 ||
  echo "esptool could not bring the AP back, see ap-up.log" >> "$O/driver.txt"
wait $CAP
cat "$O/driver.txt"
grep -a -E 'EV (ssid|channel|wifi)|Beacon Timeout|Connecting to' "$O/serial.txt" | cut -c1-150
grep -q "esptool could not" "$O/driver.txt" && { echo "FAIL: the bench AP was not moved"; exit 1; }
# only after the first channel update: the capture starts with a reset of its own
R=$(sed -n '/EV channel/,$p' "$O/serial.txt" | grep -a -c -E 'Guru|abort\(\)|Backtrace|rst:0x|task_wdt')
[ "$R" = 0 ] || { echo "FAIL: $R reset or task watchdog lines"; exit 1; }
CH=$(grep -a -o 'EV channel [0-9]* (component reports [0-9-]*, connected 1)' "$O/serial.txt" | awk '{print $3}' | uniq)
echo "connected channels in order: $(echo $CH)"
[ "$(echo "$CH" | head -1)" = "$BC" ] || { echo "FAIL: did not start on the bench AP channel $BC"; exit 1; }
echo "$CH" | grep -q -v -x "$BC" || { echo "FAIL: the channel never moved to the other network"; exit 1; }
[ "$(echo "$CH" | tail -1)" = "$BC" ] || { echo "FAIL: did not end back on the bench AP channel $BC"; exit 1; }
echo PASS
