#!/bin/bash
# run.sh LABEL: build hub.yaml as hubsim<LABEL> from TREE, flash a D1 mini over serial, read its log over the API
# until DONE (UART0 is looped back for rdm6300, so the serial port carries no log) and write out-hubsim<LABEL>.ev.
# Diff the .ev files of two builds: every automation, its arguments and the order must match.
# env: TREE (esphome checkout), PORT (D1 mini serial port), PY (checkout python); secrets.yaml next to hub.yaml
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; NAME=hubsim$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
printf 'substitutions:\n  name: %s\n\npackages:\n  hub: !include hub.yaml\n' "$NAME" > "$R/$NAME.yaml"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
# The script starts 35 s after boot; the log client must be connected by then.
(cd "$T" && PYTHONUNBUFFERED=1 timeout 90 $PY -m esphome logs "$R/$NAME.yaml" --device "$NAME.local" > "$OUT/api.txt" 2>&1)
grep -a -E "\[ev|sim:|W\]" "$OUT/api.txt" | sed -E 's/^\[[0-9:.]+\]//' > "$R/out-$NAME.ev"
EV=$(grep -c '\[ev' "$R/out-$NAME.ev"); S=$(grep -c START "$R/out-$NAME.ev"); D=$(grep -c DONE "$R/out-$NAME.ev")
echo "$NAME: $EV events, START $S, DONE $D"
[ "$S" -ge 1 ] && [ "$D" -ge 1 ] && [ "$EV" -gt 0 ] && echo PASS || { echo "FAIL: missing START, DONE or events"; exit 1; }
