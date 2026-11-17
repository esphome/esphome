#!/bin/bash
# run_all.sh TREE NAME PORT: make run_NAME.yaml (xball.yaml + xball_entities.yaml), build it from TREE, flash over USB,
# capture 75 s from a reset and count the publishes. Run python gen_all.py first.
# env: BUILD_LOCK (optional), PY (default TREE's venv python)
D=$(cd "$(dirname "$0")" && pwd); T=${1:?tree}; N=${2:?name}; P=${3:?port}
export PY=${PY:-$T/venv/bin/python}
SC=$(dirname "$D")/serial_capture.py; B=$(dirname "$D")/pio_build.sh
{ sed "s/^  name: xball$/  name: $N/" "$D/xball.yaml"; cat "$D/xball_entities.yaml"; } > "$D/run_$N.yaml"
TREE=$T bash $B compile "$N" "$D/run_$N.yaml" || exit 1
pkill -f "[s]erial_capture.py $P "
TREE=$T bash $B upload "$N" "$D/run_$N.yaml" "$P" || exit 1
sleep 3
$PY "$SC" "$P" 75 "$D/$N-cap.txt" --reset > /dev/null 2>&1
COUNT=$($PY "$D/count_all.py" "$D/$N-cap.txt"); RC=$?
echo "$COUNT" | tail -2
grep -a "IDLE" "$D/$N-cap.txt"
CRASH=$(grep -a -c 'Guru\|abort()\|Backtrace' "$D/$N-cap.txt")
echo "crash lines: $CRASH"
[ "$CRASH" -eq 0 ] || RC=1
exit $RC
