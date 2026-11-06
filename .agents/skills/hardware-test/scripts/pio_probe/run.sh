#!/bin/bash
# run.sh pa|pc LABEL: build pa.yaml (five rp2040_pio_led_strip strips on PIO0/PIO1, WS2812 and custom timings) or
# pc.yaml (one strip after on_boot claims DMA channels 0 to 11) from TREE as <cfg><LABEL>, flash the Pico 2 W, capture
# 40 s and print the probe RESULT lines. No LEDs or wires: pioscope.hpp samples each strip's own pin from PIO2.
# Every strip is set to Color(0xA5, 0x3C, 0x0F), so a correct GRB frame of two LEDs reads 3ca50f3ca50f.
# env: TREE (esphome checkout), PORT (Pico serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); C=${1:?pa or pc}; L=${2:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=$C$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
[ -f "$R/$C.yaml" ] || { echo "no $C.yaml"; exit 1; }
sed -E "s/^  name: $C$/  name: $NAME/" "$R/$C.yaml" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
# USB CDC: no reset line, the capture follows the port back after the flash reboot
$PY "$SC" "$P" 40 "$OUT/serial.txt" > /dev/null 2>&1
[ -s "$OUT/serial.txt" ] || { echo "serial capture got nothing (a panic before USB comes up leaves no port: BOOTSEL)"; exit 1; }
grep -a -o "DMA claimed.*" "$OUT/serial.txt"
grep -a -o "RESULT .*" "$OUT/serial.txt" | sort | uniq -c
N=$(grep -a -c "RESULT " "$OUT/serial.txt"); G=$(grep -a -i -c "bytes 3ca50f3ca50f" "$OUT/serial.txt")
echo "$NAME: $N results, $G with the expected frame"
grep -a -q DONE "$OUT/serial.txt" || { echo "FAIL: no DONE"; exit 1; }
[ "$N" -gt 0 ] || { echo "FAIL: no RESULT lines"; exit 1; }
