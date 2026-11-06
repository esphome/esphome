#!/bin/bash
# run.sh bme280|bmp280 LABEL [SECONDS]: build dut-common.yaml plus dut-<kind>.yaml as i2cfake<kind><LABEL> from TREE,
# flash the ESP32-C3 DUT, then capture the DUT and the Pico 2 W fake together (default 60 s, so both fake modes
# show up at least twice). Flash the fake first with picofake.yaml and the matching chip id:
#   esphome -s chip_id 0x60 -s name_sfx e run picofake.yaml   (BME280)
#   esphome -s chip_id 0x58 -s name_sfx p run picofake.yaml   (BMP280)
# Writes out-i2cfake<kind><LABEL>/{dut,fake}.txt and prints the distinct values per sensor; diff them between trees.
# env: TREE (esphome checkout), PORT (C3 serial port), FAKE_PORT (Pico serial port), SC (path to serial_capture.py),
# PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); K=${1:?bme280 or bmp280}; L=${2:?label}; S=${3:-60}
T=${TREE:?set TREE}; P=${PORT:?set PORT}; FP=${FAKE_PORT:?set FAKE_PORT}; PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}
[ -f "$R/dut-$K.yaml" ] || { echo "no dut-$K.yaml"; exit 1; }
NAME=i2cfake$K$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
printf 'esphome:\n  name: %s\n\npackages:\n  common: !include dut-common.yaml\n  sensors: !include dut-%s.yaml\n' "$NAME" "$K" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"; pkill -f "[s]erial_capture.py $FP"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
$PY "$SC" "$FP" $((S + 3)) "$OUT/fake.txt" > /dev/null 2>&1 &
FAKE_CAP=$!
$PY "$SC" "$P" "$S" "$OUT/dut.txt" --reset > /dev/null 2>&1 || { echo "FAIL: DUT capture got nothing"; exit 1; }
wait $FAKE_CAP || { echo "FAIL: fake capture got nothing (FAKE_PORT right?)"; exit 1; }
grep -a -q "STAT mode=" "$OUT/fake.txt" || { echo "FAIL: no STAT lines from the fake (flashed? FAKE_PORT right?)"; exit 1; }
grep -a -o "STAT .*" "$OUT/fake.txt" | tail -1
# "'AT': Sending state 25.08200 °C ..." -> per sensor, the distinct values seen
VALS=$(grep -a -o -E "'[A-Z]{2}': Sending state [^ ]+" "$OUT/dut.txt" | sed -E "s/'([A-Z]{2})': Sending state /\1 /" | sort | uniq -c)
[ -n "$VALS" ] || { echo "FAIL: no sensor states (check the wiring: C3 GPIO6/7 to Pico GP4/GP5 plus GND)"; exit 1; }
echo "$VALS"
case $K in bme280) WANT="AT AP AH BT BP BH" ;; *) WANT="AT AP BT BP" ;; esac
MISSING=$(for s in $WANT; do echo "$VALS" | grep -q " $s " || printf '%s ' "$s"; done)
[ -z "$MISSING" ] || { echo "FAIL: no states from $MISSING"; exit 1; }
