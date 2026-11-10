#!/bin/bash
# run.sh VARIANT LABEL: build VARIANT (int-od, int-pp or no-int) from TREE as mcp<VARIANT><LABEL>, flash the
# ESP8266 and capture 14 s into out-mcp<VARIANT><LABEL>.txt, then print the mcptest SUMMARY and IOCONB_TEST lines.
# The esptool reset keeps the expander powered, so a reflash shows whether register bits from the last build survive.
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); V=${1:?variant}; L=${2:?label}
T=${TREE:?set TREE}; P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; N=mcp${V//-/}$L
case $V in
  int-od) F=int.yaml; X=(-s open_drain true) ;;
  int-pp) F=int.yaml; X=(-s open_drain false) ;;
  no-int) F=no-int.yaml; X=() ;;
  *) echo "VARIANT is int-od, int-pp or no-int"; exit 1 ;;
esac
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome -s name "$N" "${X[@]}" run --no-logs "$R/$F" --device "$P" > "$R/out-$N-upload.log" 2>&1) ||
  { echo "build or upload failed"; tail -5 "$R/out-$N-upload.log"; exit 1; }
$PY "$SC" "$P" 14 "$R/out-$N.txt" > /dev/null 2>&1
[ -s "$R/out-$N.txt" ] || { echo "serial capture got nothing"; exit 1; }
grep -a "mcptest\|\[E\]\|\[W\]\|rst cause" "$R/out-$N.txt" | cut -c1-400
grep -a -q "SUMMARY" "$R/out-$N.txt" || { echo "FAIL: no SUMMARY line"; exit 1; }
grep -a -q "SUMMARY.*loopback_ok=20/20" "$R/out-$N.txt" || { echo "FAIL: loopback_ok is not 20/20"; exit 1; }
! grep -a -q "\[E\]" "$R/out-$N.txt" || { echo "FAIL: [E] line in the capture"; exit 1; }
# the boot after the upload may show one ROM line; a second one is a reset during the test
[ "$(grep -a -c "rst cause" "$R/out-$N.txt")" -le 1 ] || { echo "FAIL: reset during the capture"; exit 1; }
