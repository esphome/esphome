#!/bin/bash
# run.sh CASE...: generate each cases.txt case (gen_case.py), build it from TREE under one name per board (so builds
# stay incremental), flash, capture 15 s and print its STATUS line and the RMT driver errors. No LEDs needed:
# allocation failures show at boot.
# env: TREE (esphome checkout), PORT_S3 and PORT_C6 (serial ports), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}
[ $# -gt 0 ] || { echo "usage: run.sh CASE..."; exit 1; }
OUT=$R/out; mkdir -p "$OUT"; MISSING=""
for c in "$@"; do
  b=$(awk -v c="$c" '$1 == c {print $2}' "$R/cases.txt")
  case $b in s3) P=${PORT_S3:?set PORT_S3};; c6) P=${PORT_C6:?set PORT_C6};; *) echo "$c: not in cases.txt"; exit 1;; esac
  python3 "$R/gen_case.py" "$c" > "$R/rmt$b.yaml" || exit 1
  pkill -f "[s]erial_capture.py $P"
  (cd "$T" && $PY -m esphome run --no-logs "$R/rmt$b.yaml" --device "$P" > "$OUT/$c-upload.log" 2>&1) || { echo "$c build or upload failed"; MISSING="$MISSING $c"; continue; }
  if [ $b = c6 ]; then
    $PY "$SC" "$P" 15 "$OUT/$c-serial.txt" --reset > /dev/null 2>&1
  else
    # USB console: the boot lines before the port reopens are lost, a reset would only lose them again
    sleep 1; $PY "$SC" "$P" 15 "$OUT/$c-serial.txt" > /dev/null 2>&1
  fi
  S=$(grep -a -m1 -o 'STATUS.*' "$OUT/$c-serial.txt")
  [ -n "$S" ] || MISSING="$MISSING $c"
  echo "== $c: ${S:-no STATUS} | $(grep -a -ciE 'no free|Channel creation failed|mem_block|out of RMT' "$OUT/$c-serial.txt") rmt errors"
  grep -a -iE 'no free|creation failed|mem_block|out of RMT|rmt.*(E|W)\]' "$OUT/$c-serial.txt" | sort -u | cut -c1-200 | head -4
done
[ -z "$MISSING" ] || { echo "FAIL: no STATUS line for$MISSING"; exit 1; }
