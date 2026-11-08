#!/bin/bash
# run.sh TREE PORT CONFIG [SECONDS]: flash CONFIG built from TREE and capture SECONDS (default 640) from a reset
set -u
TREE=$1; PORT=$2; CFG=$(cd "$(dirname "$3")" && pwd)/$(basename "$3"); SECS=${4:-640}
SKILL=$(cd "$(dirname "$0")/../.." && pwd); PY=${PY:-python3}; N=$(basename "$CFG" .yaml); OUT=$(dirname "$CFG")
(cd "$TREE" && PYTHONPATH=$TREE $PY -m esphome upload "$CFG" --device "$PORT" > "$OUT/up-$N.log" 2>&1) || { echo "upload failed"; exit 1; }
$PY "$SKILL/scripts/serial_capture.py" "$PORT" "$SECS" "$OUT/cap-$N.txt" --reset > /dev/null 2>&1
"$(dirname "$0")/summary.sh" "$OUT/cap-$N.txt"
