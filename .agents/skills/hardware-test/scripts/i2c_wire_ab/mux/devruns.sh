#!/bin/bash
# devruns.sh NAME YAML PORT [SECONDS]: upload YAML as NAME from TREE, then three captures from a reset into
# NAME-ser.txt, -ser2 and -ser3 (what mxmaj.py reads); prints each MXDONE line. env: TREE, PY, PIO_CORE
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); cd "$D" || exit 1
N=${1:?name}; Y=${2:?yaml}; P=${3:?port}; SEC=${4:-240}; PY=${PY:-${TREE:?set TREE}/venv/bin/python}
PIO_CORE=${PIO_CORE:-$D/../pio} bash "$S/pio_build.sh" upload "$N" "$Y" "$P" || exit 1
bad=0
for r in "" 2 3; do
  f=$N-ser$r.txt; rm -f "$f" "$f.failed"
  # A failed or unfinished capture is moved aside so mxmaj.py never reads it as a complete run.
  if ! $PY "$S/serial_capture.py" "$P" "$SEC" "$f" --reset > /dev/null; then
    echo "$f: capture failed" >&2
  elif ! grep -a -o "MXDONE.*" "$f"; then
    echo "$f: no MXDONE" >&2
  else
    continue
  fi
  mv -f "$f" "$f.failed" 2> /dev/null; bad=$((bad + 1))
done
[ "$bad" = 0 ] || { echo "$bad of 3 captures failed, kept as *.failed" >&2; exit 1; }
