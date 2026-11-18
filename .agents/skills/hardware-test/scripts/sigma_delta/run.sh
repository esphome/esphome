#!/bin/bash
# run.sh LABEL [gen|gen2] [SECONDS]: generate the sequence (gen.py by default) into sd<LABEL>.yaml, build it from TREE,
# flash the ESP32-P4, capture with --ms until the run ends (default 240 s) and print the RES lines and an.py's fits.
# Nothing is wired to GPIO21; sdprobe.hpp reads the pin from inside the chip.
# env: TREE (esphome checkout), PORT (P4 serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; G=${2:-gen}; S=${3:-240}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=sd$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
python3 "$R/$G.py" "$R/tpl.yaml" > "$R/$NAME.yaml" || { echo "$G.py failed"; exit 1; }
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome -s name "$NAME" run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
$PY "$SC" "$P" "$S" "$OUT/serial.txt" --reset --ms > /dev/null 2>&1
[ -s "$OUT/serial.txt" ] || { echo "serial capture got nothing"; exit 1; }
grep -a -o -E "RES .*" "$OUT/serial.txt"
grep -a -q ALLDONE "$OUT/serial.txt" || { echo "FAIL: no ALLDONE (raise SECONDS?)"; exit 1; }
[ "$G" = gen ] || exit 0
python3 "$R/an.py" "$OUT/serial.txt"
