#!/bin/bash
# run.sh bk72xx|esp8266|esp32 LABEL: build the on_value_range sequence as rng<kind><LABEL> from TREE, flash it,
# capture 90 s (the sequence restarts the board once) and write out-rng<kind><LABEL>.seq; diff the .seq files of
# dev and the PR. Each build gets its own tag, so a reflash always runs the full sequence from phase 0.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python),
# BOARD (bk72xx board, default xh-wb3s), BKPY + BKFLASH (python and flash_bk72xx.py for bk72xx)
R=$(cd "$(dirname "$0")" && pwd); K=${1:?bk72xx, esp8266 or esp32}; L=${2:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=rng$K$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
TAG=1$(date +%s | tail -c 6)  # leading 1: a leading 0 would make an octal literal
case $K in
  bk72xx) PLATFORM=$(printf 'bk72xx:\n  board: %s\nlogger:\n  hardware_uart: UART1' "${BOARD:-xh-wb3s}");;
  esp8266) PLATFORM=$'esp8266:\n  board: d1_mini';;
  *) PLATFORM=$'esp32:\n  board: esp32dev\n  framework:\n    type: esp-idf';;
esac
printf 'substitutions:\n  tag: "%s"\nesphome:\n  name: %s\n%s\npackages:\n  rng: !include value-range-common.yaml\n' "$TAG" "$NAME" "$PLATFORM" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome compile "$R/$NAME.yaml" > "$OUT/compile.log" 2>&1) || { echo "build failed"; exit 1; }
grep -a -E '(RAM|Flash):' "$OUT/compile.log"
if [ "$K" = bk72xx ]; then
  ${BKPY:?set BKPY} "${BKFLASH:?set BKFLASH}" flash write -d "$P" "$R/.esphome/build/$NAME/.pioenvs/$NAME/firmware.uf2" > "$OUT/flash.log" 2>&1 || { echo "flash failed"; exit 1; }
  $PY "$SC" "$P" 90 "$OUT/serial.txt" > /dev/null 2>&1
else
  (cd "$T" && $PY -m esphome upload "$R/$NAME.yaml" --device "$P" > "$OUT/flash.log" 2>&1) || { echo "flash failed"; exit 1; }
  $PY "$SC" "$P" 90 "$OUT/serial.txt" --reset > /dev/null 2>&1
fi
# Log line numbers differ between trees, so strip the [tag:NNN] part along with the time stamps.
grep -a -E "\[fire|\[seq" "$OUT/serial.txt" | sed -E 's/^\[[0-9:.]*\]//; s/\[(fire|seq):[0-9]+\]/[\1]/' > "$R/out-$NAME.seq"
F=$(grep -c fire "$R/out-$NAME.seq"); D=$(grep -c DONE "$R/out-$NAME.seq")
echo "$NAME: $F fires, DONE $D"
[ "$D" -ge 1 ] || { echo "FAIL: no DONE (did the restart phase run?)"; exit 1; }
# Fires per trigger for this sequence (dev on ESP8266, ESP32 and BK72xx); S6 has NaN bounds and never fires
BAD=$(sed -nE 's/.*\[fire\]: ([A-Z][0-9]+) .*/\1/p' "$R/out-$NAME.seq" | sort | uniq -c | awk '{n[$2] = $1} END {
  split("S1:6 S2:5 S3:3 S4:5 S5:2 S6:0 S7:6 N1:6 N2:4 N3:2", e, " ")
  for (i in e) { split(e[i], kv, ":"); if (n[kv[1]] + 0 != kv[2]) printf "%s fired %d times, expected %d; ", kv[1], n[kv[1]], kv[2] }
}')
[ -z "$BAD" ] && echo PASS || { echo "FAIL: $BAD"; exit 1; }
