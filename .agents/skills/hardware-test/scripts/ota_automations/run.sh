#!/bin/bash
# run.sh esp32|esp8266 LABEL: build otaauto<kind><LABEL> from TREE, flash it by serial, then run four OTA cases under
# one capture: a native upload, a native upload whose uploader is SIGKILLed mid-transfer, a web_server upload of a
# random 64 KB file and a web_server upload of the real image. summ.py prints the automation fires per case.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); K=${1:?esp32 or esp8266}; L=${2:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=otaauto$K$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
if [ "$K" = esp32 ]; then PLATFORM=$'esp32:\n  board: esp32dev\n  framework:\n    type: esp-idf'; else PLATFORM=$'esp8266:\n  board: d1_mini'; fi
printf 'esphome:\n  name: %s\n%s\npackages:\n  ota: !include ota-automations-common.yaml\n' "$NAME" "$PLATFORM" > "$R/$NAME.yaml"
M=$OUT/markers.txt; mark() { echo "$(date +%H:%M:%S) $*" >> "$M"; echo "$(date +%T) $*"; }
up() { (cd "$T" && $PY -m esphome upload "$R/$NAME.yaml" --device "$1"); }
(cd "$T" && $PY -m esphome compile "$R/$NAME.yaml" > "$OUT/compile.log" 2>&1) || { echo "build failed"; exit 1; }
grep -a -E '(RAM|Flash):' "$OUT/compile.log"
pkill -f "[s]erial_capture.py $P"
up "$P" > "$OUT/flash.log" 2>&1 || { echo "flash failed"; exit 1; }
$PY "$SC" "$P" 900 "$OUT/serial.txt" --reset > /dev/null 2>&1 & CAP=$!
IP=""; for _ in $(seq 60); do IP=$(LC_ALL=C grep -a -o "IP Address: [0-9.]*" "$OUT/serial.txt" 2>/dev/null | tail -1 | cut -d' ' -f3); [ -n "$IP" ] && break; sleep 2; done
[ -n "$IP" ] || { echo "no IP in the capture"; kill $CAP; exit 1; }
sleep 10
BIN=$(ls "$R/.esphome/build/$NAME/build/firmware.ota.bin" 2>/dev/null || ls "$R/.esphome/build/$NAME/.pioenvs/$NAME/firmware.bin")
head -c 65536 /dev/urandom > "$OUT/random64k.bin"
mark "CASE1 native normal"; up "$IP" > "$OUT/case1.log" 2>&1; mark "RESULT1 rc $?"; sleep 45
mark "CASE2 native killed"; START=$(wc -l < "$OUT/serial.txt"); up "$IP" > "$OUT/case2.log" 2>&1 &
SEEN=no; for _ in $(seq 40); do tail -n +$((START + 1)) "$OUT/serial.txt" | LC_ALL=C grep -a -q "AUTO native progress" && { SEEN=yes; break; }; sleep 0.5; done
if [ $SEEN = yes ]; then sleep 3; pkill -9 -f "esphome upload $R/$NAME.yaml --device $IP"; mark "RESULT2 progress yes pkill $?"
else pkill -9 -f "esphome upload $R/$NAME.yaml --device $IP"; mark "RESULT2 progress no"; fi; sleep 45
mark "CASE3 web random file"
mark "RESULT3 http $(curl -s -m 60 -o /dev/null -w '%{http_code}' -F "update=@$OUT/random64k.bin" "http://$IP/update")"; sleep 20
mark "CASE4 web normal"
mark "RESULT4 http $(curl -s -m 300 -o /dev/null -w '%{http_code}' -F "update=@$BIN" "http://$IP/update")"; sleep 45
mark "END"; kill $CAP
$PY "$R/summ.py" "$OUT" && echo PASS || { echo FAIL; exit 1; }
