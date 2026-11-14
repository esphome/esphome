#!/bin/bash
# run.sh CONFIG LABEL [SECONDS]: build CONFIG (can-loopback, can-classic or can-normal-busoff) from TREE as
# <CONFIG>-<LABEL>, flash the PICO-D4, capture SECONDS (default 140) plus two more resets for the boot heap line.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); C=${1:?config}; L=${2:?label}; S=${3:-140}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=$C-$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
sed -E "s/^  name: .*/  name: $NAME/" "$R/$C.yaml" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
$PY "$SC" "$P" "$S" "$OUT/serial.txt" --reset > /dev/null 2>&1 || { echo "serial capture got nothing"; exit 1; }
for b in 2 3; do $PY "$SC" "$P" 8 "$OUT/boot$b.txt" --reset > /dev/null 2>&1 || echo "boot$b capture got nothing"; done
grep -a -h -o "BOOTFREE [0-9]* TOTAL [0-9]*" "$OUT/serial.txt" "$OUT/boot2.txt" "$OUT/boot3.txt"
grep -a -E "canlb|mcp251xfd" "$OUT/serial.txt" | grep -a -v "failed with error" | sed 's/^.*\]\[/[/' | cut -c1-200
grep -a -q "PHASES DONE" "$OUT/serial.txt" || { echo "FAIL: phases did not finish within $S s"; exit 1; }
echo "spi_device lines: $(grep -a -c spi_device "$OUT/serial.txt")  failed sends: $(grep -a -c 'failed with error' "$OUT/serial.txt")"
