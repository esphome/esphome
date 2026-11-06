#!/bin/bash
# run.sh esp8266|bk72xx LABEL: build the filter A/B firmware sf<kind><LABEL> from TREE, flash it, capture until DONE
# and write out-sf<kind><LABEL>.out (one line per input with every filter output as float bits). Compare two builds
# with cmp: identical files mean identical filter behaviour, dropped values included.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python),
# BASELINE (the delta baseline under test, default `!lambda return 20.0f;`; a plain number crashes before #21811),
# BOARD (bk72xx board, default xh-wb3s), BKPY + BKFLASH (python and flash_bk72xx.py for bk72xx)
R=$(cd "$(dirname "$0")" && pwd); K=${1:?esp8266 or bk72xx}; L=${2:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; NAME=sf$K$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
printf 'substitutions:\n  name: %s\n  board: %s\n  baseline: %s\n\npackages:\n  board: !include board-%s.yaml\n  sf: !include sf.yaml\n' \
  "$NAME" "${BOARD:-xh-wb3s}" "${BASELINE:-!lambda return 20.0f;}" "$K" > "$R/$NAME.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome compile "$R/$NAME.yaml" > "$OUT/compile.log" 2>&1) || { echo "build failed"; exit 1; }
grep -a -E '(RAM|Flash):' "$OUT/compile.log"
if [ "$K" = esp8266 ]; then
  # The esptool stub can hit serial noise on a CH340: retry.
  for try in 1 2 3; do
    $PY -m esptool --port "$P" -b 115200 write-flash 0x0 "$R/.esphome/build/$NAME/.pioenvs/$NAME/firmware.bin" > "$OUT/flash.log" 2>&1 && break
    [ $try = 3 ] && { echo "flash failed"; exit 1; }; sleep 2
  done
  $PY "$SC" "$P" 40 "$OUT/serial.txt" --reset > /dev/null 2>&1
else
  for try in 1 2; do
    ${BKPY:?set BKPY} "${BKFLASH:?set BKFLASH}" flash write -d "$P" "$R/.esphome/build/$NAME/.pioenvs/$NAME/firmware.uf2" > "$OUT/flash.log" 2>&1 && break
    [ $try = 2 ] && { echo "flash failed"; exit 1; }
  done
  $PY "$SC" "$P" 40 "$OUT/serial.txt" > /dev/null 2>&1
fi
grep -a -o '[ij]=[0-9][0-9] .*' "$OUT/serial.txt" | sed 's/\x1b\[[0-9;]*m//g' > "$R/out-$NAME.out"
LINES=$(wc -l < "$R/out-$NAME.out"); DONE=$(grep -a -c DONE "$OUT/serial.txt")
echo "$NAME: $LINES output lines, DONE $DONE"
[ "$DONE" -ge 1 ] && [ "$LINES" = 100 ] && echo PASS || { echo "FAIL: want DONE and 100 lines (50 inputs, sensor and binary/text lines)"; exit 1; }
