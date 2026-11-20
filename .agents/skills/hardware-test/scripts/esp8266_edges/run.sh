#!/bin/bash
# run.sh YAML NAME [SECONDS]: build YAML as NAME, flash it over serial and capture from reset into NAME-cap.txt.
# env: TREE (checkout under test), PORT (the board's serial port), PY (default $TREE/venv/bin/python)
S=$(cd "$(dirname "$0")" && pwd); Y=${1:?yaml}; N=${2:?name}; SEC=${3:-80}
: "${TREE:?set TREE}"; P=${PORT:?set PORT}; PY=${PY:-$TREE/venv/bin/python}
"$S/../pio_build.sh" compile "$N" "$Y" || exit 1
"$S/../pio_build.sh" upload "$N" "$Y" "$P" || exit 1
"$PY" "$S/../serial_capture.py" "$P" "$SEC" "$S/$N-cap.txt" --reset > /dev/null || exit 1
echo "captured $S/$N-cap.txt"
