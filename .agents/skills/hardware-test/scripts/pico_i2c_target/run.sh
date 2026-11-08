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
# the fake capture starts first; its last STAT line before the DUT reset is the baseline for abrt and bad, and it
# runs until a STAT line arrives after the DUT capture ends, so the comparison covers the whole DUT run
$PY "$SC" "$FP" $((S + 60)) "$OUT/fake.txt" > /dev/null 2>&1 &
FAKE_CAP=$!
for _ in $(seq 20); do grep -a -q "STAT mode=" "$OUT/fake.txt" 2> /dev/null && break; sleep 0.5; done
BASE=$(grep -a -o "STAT .*" "$OUT/fake.txt" 2> /dev/null | tail -1)
[ -n "$BASE" ] || { kill $FAKE_CAP 2>/dev/null; echo "FAIL: no STAT line from the fake before the DUT reset (flashed? FAKE_PORT right?)"; exit 1; }
$PY "$SC" "$P" "$S" "$OUT/dut.txt" --reset > /dev/null 2>&1 || { kill $FAKE_CAP 2>/dev/null; echo "FAIL: DUT capture got nothing"; exit 1; }
NEND=$(grep -a -c "STAT mode=" "$OUT/fake.txt")
for _ in $(seq 30); do
  [ "$(grep -a -c "STAT mode=" "$OUT/fake.txt")" -gt "$NEND" ] && break
  kill -0 $FAKE_CAP 2> /dev/null || break
  sleep 0.5
done
kill $FAKE_CAP 2> /dev/null; wait $FAKE_CAP 2> /dev/null
[ "$(grep -a -c "STAT mode=" "$OUT/fake.txt")" -gt "$NEND" ] || { echo "FAIL: no STAT line from the fake after the DUT capture ended (FAKE_PORT right?)"; exit 1; }
LAST=$(grep -a -o "STAT .*" "$OUT/fake.txt" | tail -1)
echo "$LAST"
# abrt: a read reply that never reached the DUT; bad: a read or write of a register the fake does not model
cnt() { echo "$1" | sed -nE "s/.* $2=([0-9]+).*/\1/p"; }
for c in abrt bad; do
  B=$(cnt "$BASE" $c); E=$(cnt "$LAST" $c)
  [ -n "$B" ] && [ -n "$E" ] || { echo "FAIL: no $c count on the STAT lines (fake built from this picofake.hpp?)"; exit 1; }
  eval "D_$c=$((E - B))"
done
# "'AT': Sending state 25.08200 °C ..." -> per sensor, the distinct values seen
VALS=$(grep -a -o -E "'[A-Z]{2}': Sending state [^ ]+" "$OUT/dut.txt" | sed -E "s/'([A-Z]{2})': Sending state /\1 /" | sort | uniq -c)
[ -n "$VALS" ] || { echo "FAIL: no sensor states (check the wiring: C3 GPIO6/7 to Pico GP4/GP5 plus GND)"; exit 1; }
echo "$VALS"
NUM=$(echo "$VALS" | grep -v -i " nan$")
NANS=$(echo "$VALS" | grep -i " nan$")
[ -z "$NANS" ] || echo "NaN states: $(echo "$NANS" | awk '{printf "%s=%s ", $2, $1}')"
case $K in bme280) WANT="AT AP AH BT BP BH" ;; *) WANT="AT AP BT BP" ;; esac
MISSING=$(for s in $WANT; do echo "$NUM" | grep -q " $s " || printf '%s ' "$s"; done)
[ -z "$MISSING" ] || { echo "FAIL: no numeric states from $MISSING"; exit 1; }
[ "$D_abrt" = 0 ] || { echo "FAIL: the fake aborted $D_abrt read replies during the run"; exit 1; }
[ "$D_bad" = 0 ] || { echo "FAIL: $D_bad reads or writes of registers the fake does not model during the run"; exit 1; }
echo PASS
