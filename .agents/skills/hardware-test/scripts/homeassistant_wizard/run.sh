#!/bin/bash
# run.sh LABEL: build ha-all-common.yaml from TREE as hawiz<LABEL> for a D1 mini, flash, take three idle boots of heap
# readings (ramchk lines), then drive one boot with hadrive.py and print its result and the heap after the client.
# NO_LIGHT=1 drops the wizard-linked light, which builds before #21908 cannot compile.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; N=hawiz$L
T=${TREE:?set TREE}; P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}
OUT=$R/out-$N; rm -rf "$OUT"; mkdir -p "$OUT"
LIGHT='  light: !include ha-wiz-light.yaml\n'; NL=(); [ -n "$NO_LIGHT" ] && { LIGHT=; NL=(--no-light); }
printf "esphome:\n  name: %s\n\nesp8266:\n  board: d1_mini\n\npackages:\n  common: !include ha-all-common.yaml\n$LIGHT" "$N" \
  > "$R/$N.yaml"
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome run --no-logs "$R/$N.yaml" --device "$P" > "$OUT/upload.log" 2>&1) ||
  { echo "build or upload failed"; tail -5 "$OUT/upload.log"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
for b in 1 2 3; do
  $PY "$SC" "$P" 45 "$OUT/boot$b.txt" --reset > /dev/null 2>&1
  echo "boot$b $(grep -a 'ramchk' "$OUT/boot$b.txt" | tail -1 | sed -E 's/.*(heap_size)/\1/')"
done
$PY "$SC" "$P" 150 "$OUT/serial.txt" --reset > /dev/null 2>&1 & CAP=$!
for _ in $(seq 1 30); do
  IP=$(grep -a -o "IP Address: [0-9.]*" "$OUT/serial.txt" 2>/dev/null | tail -1 | cut -d' ' -f3)
  [ -n "$IP" ] && break; sleep 2
done
[ -n "$IP" ] || { echo "no IP in the capture"; kill $CAP; exit 1; }
sleep 8
$PY "$R/hadrive.py" "$IP" "${NL[@]}" > "$OUT/drive.txt" 2>&1; RC=$?
sleep 6; kill $CAP
echo "drive rc=$RC $(tail -1 "$OUT/drive.txt" | cut -c1-300)"
echo "after-client $(grep -a 'ramchk' "$OUT/serial.txt" | tail -1 | sed -E 's/.*(heap_size)/\1/')"
echo "crash lines: $(grep -a -c -E 'rst cause|Exception|Soft WDT' "$OUT/serial.txt")"
exit $RC
