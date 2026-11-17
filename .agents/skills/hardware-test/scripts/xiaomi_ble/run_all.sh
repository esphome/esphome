#!/bin/bash
# run_all.sh TREE NAME PORT: make NAME.yaml (xball.yaml + xball_entities.yaml), build it from TREE, flash over USB,
# capture 75 s from a reset and count the publishes. Run python gen_all.py first.
# env: BUILD_LOCK (optional), PY (default TREE's venv python)
D=$(cd "$(dirname "$0")" && pwd); T=${1:?tree}; N=${2:?name}; P=${3:?port}
export PIO_CORE=${PIO_CORE:-$D/pio} PY=${PY:-$T/venv/bin/python}
SC=$(dirname "$D")/serial_capture.py; B=$(dirname "$D")/pio_build.sh
{ sed "s/^  name: xball$/  name: $N/" "$D/xball.yaml"; cat "$D/xball_entities.yaml"; } > "$D/$N.yaml"
TREE=$T bash $B compile "$N" "$D/$N.yaml" || exit 1
pkill -f "[$(basename "$SC" | cut -c1)]$(basename "$SC" | cut -c2-) $P"
TREE=$T bash $B upload "$N" "$D/$N.yaml" "$P" || exit 1
sleep 3
$PY "$SC" "$P" 75 "$D/$N-cap.txt" --reset > /dev/null 2>&1
$PY "$D/count_all.py" "$D/$N-cap.txt" | tail -2
grep -a "IDLE" "$D/$N-cap.txt"
echo "crash lines: $(grep -a -c 'Guru\|abort()\|Backtrace' "$D/$N-cap.txt")"
