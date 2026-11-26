#!/bin/bash
# run.sh BOARD LABEL SECONDS: build spi<BOARD>.yaml (e32, c3, e66 or rp; gen.py writes them) from TREE as
# spi<BOARD><LABEL>, flash, capture SECONDS (three passes, then the board reboots) into out-spi<BOARD><LABEL>.txt and
# check it with analyze.py. Compare two builds with: analyze.py spi<BOARD>.yaml out-<base>.txt out-<pr>.txt
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (checkout python),
# PORTS (path to ports.py, Pico only: the port comes back after the upload)
R=$(cd "$(dirname "$0")" && pwd); B=${1:?board}; L=${2:?label}; S=${3:?seconds}
T=${TREE:?set TREE}; P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; N=spi$B$L
[ -f "$R/spi$B.yaml" ] || $PY "$R/gen.py" > /dev/null
[ -f "$R/spi$B.yaml" ] || { echo "no spi$B.yaml"; exit 1; }
pkill -f "[s]erial_capture.py $P"
(cd "$T" && $PY -m esphome -s name "$N" run --no-logs "$R/spi$B.yaml" --device "$P" > "$R/out-$N-upload.log" 2>&1) ||
  { echo "build or upload failed"; tail -5 "$R/out-$N-upload.log"; exit 1; }
grep -a -E '^(RAM|Flash):' "$R/out-$N-upload.log"
RESET=--reset
if [ "$B" = rp ]; then $PY "${PORTS:?set PORTS}" --wait 2e8a > /dev/null; sleep 1; RESET=; fi
$PY "$SC" "$P" "$S" "$R/out-$N.txt" $RESET > /dev/null 2>&1
[ -s "$R/out-$N.txt" ] || { echo "serial capture got nothing"; exit 1; }
$PY "$R/analyze.py" "$R/spi$B.yaml" "$R/out-$N.txt"
