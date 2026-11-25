#!/bin/bash
# wdtrun.sh NAME FREQ [RESETS] [SECONDS]: build tca.yaml from TREE with the root bus at FREQ, flash it to PORT, then
# RESETS captures (default 10) of SECONDS each (default 12) from a reset; per run: boots, full setups, Soft WDT resets,
# INTERNAL_STATUS 01 after setup, good readings and a failed mux. env: TREE, PORT, PY, PIO_CORE, BUILD_LOCK
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); cd "$D" || exit 1
N=${1:?name}; F=${2:?freq}; R=${3:-10}; SEC=${4:-12}; P=${PORT:?set PORT}; PY=${PY:-${TREE:?set TREE}/venv/bin/python}
export LC_ALL=C
bash "$S/pio_build.sh" compile "$N" tca.yaml -s freq "$F" || exit 1
bash "$S/pio_build.sh" upload "$N" tca.yaml "$P" -s freq "$F" || exit 1
for i in $(seq 1 "$R"); do
  f=$N-r$i-cap.txt
  rm -f "$f"
  $PY "$S/serial_capture.py" "$P" "$SEC" "$f" --reset > /dev/null || { echo "$f: capture failed" >&2; continue; }
  echo "$N run $i: boots=$(grep -a -c 'Running through setup' "$f") full_setup=$(grep -a -c 'SETUP ' "$f")" \
    "wdt=$(grep -a -c 'Soft WDT reset' "$f") is01=$(grep -a -c 'after_setup.*INTERNAL_STATUS=01' "$f")" \
    "values=$(grep -a 'VALUES' "$f" | grep -vc nan) mux_failed=$(grep -a -c 'tca9548a was marked as failed' "$f")" \
    "$(grep -a -o 'setup_us=[0-9]*' "$f" | head -1)"
done
