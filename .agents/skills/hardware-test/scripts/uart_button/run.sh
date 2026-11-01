#!/bin/bash
# run.sh LABEL HOST: build uartbtn<LABEL> for a D1 mini from TREE, flash it by USB, wait for the API at HOST (the
# serial console is the looped back bus, so take the address from DHCP or mDNS) and run ubdrive.py.
# env: TREE (esphome checkout), PORT (serial port), PY (checkout python with aioesphomeapi)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; H=${2:?device address}; T=${TREE:?set TREE}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}; NAME=uartbtn$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
printf 'esphome:\n  name: %s\nesp8266:\n  board: d1_mini\npackages:\n  ub: !include uart-button-common.yaml\n' "$NAME" > "$R/$NAME.yaml"
(cd "$T" && $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
# ubdrive.py exits 2 when the TX or RX count is short and 3 when the device cannot be reached; only 3 is retried
for _ in $(seq 30); do
  sleep 3; timeout 60 $PY "$R/ubdrive.py" "$H" "$OUT/log.txt" 2> "$OUT/drive.err"; rc=$?
  [ $rc = 0 ] && { echo PASS; exit 0; }
  [ $rc = 2 ] && { echo FAIL; exit 1; }
  [ $rc = 3 ] || { echo "FAIL: driver exited $rc"; tail -5 "$OUT/drive.err"; exit 1; }
done
echo "FAIL: no API connection"; tail -2 "$OUT/drive.err"; exit 1
