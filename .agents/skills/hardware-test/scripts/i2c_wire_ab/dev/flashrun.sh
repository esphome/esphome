#!/bin/bash
# flashrun.sh NAME YAML PORT SECONDS: upload from TREE, then capture from a reset and print the sniffed line count and
# the SNDONE line. env: TREE, PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); cd "$D" || exit 1
N=${1:?name}; Y=${2:?yaml}; P=${3:?port}; SEC=${4:?seconds}; PY=${PY:-${TREE:?set TREE}/venv/bin/python}
pkill -f "[s]erial_capture.py $P"; sleep 1
PIO_CORE=${PIO_CORE:-$D/../pio} bash "$S/pio_build.sh" upload "$N" "$Y" "$P" || exit 1
$PY "$S/serial_capture.py" "$P" "$SEC" "$N-ser.txt" --reset > /dev/null 2>&1
grep -a -c "SN " "$N-ser.txt"; grep -a "SNDONE" "$N-ser.txt"
