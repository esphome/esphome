#!/bin/bash
# picorun.sh NAME YAML PORT [OUTSUFFIX]: upload from TREE (reboots the Pico), wait for the port, capture 60 s and print
# the sweep line count, the SWDONE line and the count of wedge cases. env: TREE, PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); cd "$D" || exit 1
N=${1:?name}; Y=${2:?yaml}; P=${3:?port}; X=${4:-}; PY=${PY:-${TREE:?set TREE}/venv/bin/python}
PIO_CORE=${PIO_CORE:-$D/../pio} bash "$S/pio_build.sh" upload "$N" "$Y" "$P" > /dev/null || exit 1
$PY "$S/ports.py" --wait 2e8a > /dev/null 2>&1
$PY "$S/serial_capture.py" "$P" 60 "$N-ser$X.txt" > /dev/null 2>&1
echo "$N$X $(grep -a -c 'SW ' "$N-ser$X.txt") $(grep -a -o 'SWDONE.*' "$N-ser$X.txt")" \
  "$(grep -a -c 'WEDGE.*rc=6 then read rc=6' "$N-ser$X.txt")"
