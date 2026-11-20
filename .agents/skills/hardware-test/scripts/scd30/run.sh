#!/bin/bash
# run.sh NAME YAML PORT SECONDS: upload YAML as NAME from TREE, capture SECONDS from a reset into run-NAME.txt and
# print the ana.sh counts. env: TREE, PY, PIO_CORE
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); cd "$D" || exit 1
PY=${PY:-${TREE:?set TREE}/venv/bin/python}
bash "$S/pio_build.sh" upload "${1:?name}" "$D/${2:?yaml}" "${3:?port}" || exit 1
$PY "$S/serial_capture.py" "$3" "${4:?seconds}" "run-$1.txt" --reset > /dev/null 2>&1
bash "$D/ana.sh" "run-$1.txt"
