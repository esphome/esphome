#!/bin/bash
# devruns.sh NAME YAML PORT [SECONDS]: upload YAML as NAME from TREE, then three captures from a reset into
# NAME-ser.txt, -ser2 and -ser3 (what mxmaj.py reads); prints each MXDONE line. env: TREE, PY, PIO_CORE
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); cd "$D" || exit 1
N=${1:?name}; Y=${2:?yaml}; P=${3:?port}; SEC=${4:-240}; PY=${PY:-${TREE:?set TREE}/venv/bin/python}
PIO_CORE=${PIO_CORE:-$D/../pio} bash "$S/pio_build.sh" upload "$N" "$Y" "$P" || exit 1
for r in "" 2 3; do
  $PY "$S/serial_capture.py" "$P" "$SEC" "$N-ser$r.txt" --reset > /dev/null 2>&1
  grep -a -o "MXDONE.*" "$N-ser$r.txt" || echo "$N-ser$r.txt: no MXDONE"
done
