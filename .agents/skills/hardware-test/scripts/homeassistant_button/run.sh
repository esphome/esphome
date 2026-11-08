#!/bin/bash
# run.sh LABEL: build habtn<LABEL> for a D1 mini from TREE, flash it, capture 90 s and run habdrive.py against the
# address from the boot log; prints the action calls and the device's homeassistant.button lines.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=habtn$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
printf 'esphome:\n  name: %s\nesp8266:\n  board: d1_mini\npackages:\n  hab: !include ha-button-common.yaml\n' "$NAME" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
$PY "$SC" "$P" 90 "$OUT/serial.txt" --reset > /dev/null 2>&1 & CAP=$!
sleep 25  # Wi-Fi and the API must be up before the client connects
IP=$(LC_ALL=C grep -a -o "IP Address: [0-9.]*" "$OUT/serial.txt" | tail -1 | cut -d' ' -f3)
[ -n "$IP" ] || { echo "no IP in the capture"; kill $CAP; exit 1; }
$PY "$R/habdrive.py" "$IP" | tee "$R/out-$NAME.actions"; RC=${PIPESTATUS[0]}
sleep 2; kill $CAP
LC_ALL=C grep -a -E "homeassistant.button|api.wizard" "$OUT/serial.txt" | sed -E 's/^\[[0-9:.]*\]//'
[ "$RC" = 0 ] || { echo "FAIL: habdrive.py exited $RC"; exit 1; }
