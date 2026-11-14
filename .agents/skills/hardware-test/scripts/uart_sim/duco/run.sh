#!/bin/bash
# run.sh NAME KIND SECONDS: generate NAME.yaml with gen.py (KIND pr|dev), build it from TREE, flash the board and
# capture its log to NAME-log.txt; a pr build ends with check.py against expected.txt (exit 1 on a mismatch).
# Run it from a scratch directory that holds a copy of ducosim.hpp and secrets.yaml.
# env: TREE, PY, DUCO_PORT (the board), BUILD_LOCK (optional, see ../../pio_build.sh)
D=$PWD; S=$(cd "$(dirname "$0")/../.." && pwd); N=${1:?name}; K=${2:?pr|dev}; SECS=${3:?seconds}
P=${DUCO_PORT:?set DUCO_PORT}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
[ -f "$D/ducosim.hpp" ] || { echo "copy ducosim.hpp into $D first"; exit 1; }
$PY "$(dirname "$0")/gen.py" "$N" "$K" || exit 1
"$S/pio_build.sh" compile "$N" "$D/$N.yaml" || exit 1
pkill -f "[s]erial_capture.py $P "
"$S/pio_build.sh" upload "$N" "$D/$N.yaml" "$P" || exit 1
$PY "$S/serial_capture.py" "$P" "$SECS" "$D/$N-log.txt" --reset || { echo "FAIL: $N capture failed"; exit 1; }
# the box must have answered: an empty control row from a dead hub or a miswired UART is no control
REQ=$(grep -a -o "ALIVE requests [0-9]*" "$D/$N-log.txt" | tail -1 | grep -o "[0-9]*$")
[ "${REQ:-0}" -gt 0 ] || { echo "FAIL: the box model answered no requests"; exit 1; }
# the dev build is the control and has no expected states; a PR build must match expected.txt
[ "$K" = pr ] || { echo "$N done ($REQ requests, no state verdict for dev)"; exit 0; }
$PY "$(dirname "$0")/check.py" "$D/$N-log.txt" "$D/expected.txt"
