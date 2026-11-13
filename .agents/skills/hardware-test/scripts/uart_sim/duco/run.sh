#!/bin/bash
# run.sh NAME KIND SECONDS: generate NAME.yaml with gen.py (KIND pr|dev), build it from TREE, flash the board and
# capture its log to NAME-log.txt. Run it from a scratch directory that holds a copy of ducosim.hpp and secrets.yaml.
# env: TREE, PY, DUCO_PORT (the board), BUILD_LOCK (optional, see ../../pio_build.sh)
D=$PWD; S=$(cd "$(dirname "$0")/../.." && pwd); N=${1:?name}; K=${2:?pr|dev}; SECS=${3:?seconds}
P=${DUCO_PORT:?set DUCO_PORT}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
[ -f "$D/ducosim.hpp" ] || { echo "copy ducosim.hpp into $D first"; exit 1; }
$PY "$(dirname "$0")/gen.py" "$N" "$K" || exit 1
"$S/pio_build.sh" compile "$N" "$D/$N.yaml" || exit 1
pkill -f "[s]erial_capture.py $P "
"$S/pio_build.sh" upload "$N" "$D/$N.yaml" "$P" || exit 1
$PY "$S/serial_capture.py" "$P" "$SECS" "$D/$N-log.txt" --reset || { echo "$N capture failed"; exit 1; }
echo "$N done"
