#!/bin/bash
# run.sh YAML NAME PORT [SECONDS]: build YAML (also ../tca9548a_real/tca.yaml) as NAME from TREE, flash it,
# capture SECONDS (default 40) from a reset into NAME-cap.txt and print the setup, register and error lines plus the count of VALUES lines.
# env: TREE, PY, PIO_CORE, BUILD_LOCK
Y=$(cd "$(dirname "${1:?yaml}")" && pwd)/$(basename "$1")
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); cd "$D" || exit 1
N=${2:?name}; P=${3:?port}; SEC=${4:-40}; PY=${PY:-${TREE:?set TREE}/venv/bin/python}
bash "$S/pio_build.sh" compile "$N" "$Y" || exit 1
bash "$S/pio_build.sh" upload "$N" "$Y" "$P" || exit 1
rm -f "$N-cap.txt"
$PY "$S/serial_capture.py" "$P" "$SEC" "$N-cap.txt" --reset > /dev/null || { echo "capture failed" >&2; exit 1; }
grep -a -E "bmi270|bmiprobe|motion|rst cause|Exception|Soft WDT|i2c" "$N-cap.txt" | grep -a -v "VALUES\|MAG " | head -40
grep -a -c "VALUES" "$N-cap.txt"
