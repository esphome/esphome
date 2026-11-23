#!/bin/bash
# run.sh NAME YAML PORT SECONDS: build YAML as NAME from TREE, flash it, capture SECONDS from a reset into
# run-NAME.txt and print the ana.py summary. env: TREE, PY, PIO_CORE, BUILD_LOCK
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); cd "$D" || exit 1
PY=${PY:-${TREE:?set TREE}/venv/bin/python}
bash "$S/pio_build.sh" compile "${1:?name}" "$D/${2:?yaml}" || exit 1
bash "$S/pio_build.sh" upload "$1" "$D/$2" "${3:?port}" || exit 1
rm -f "run-$1.txt"
$PY "$S/serial_capture.py" "$3" "${4:?seconds}" "run-$1.txt" --reset > /dev/null || { echo "capture failed" >&2; exit 1; }
$PY "$D/ana.py" "run-$1.txt"
