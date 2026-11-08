#!/bin/bash
# run.sh TREE PORT CONFIG [SECONDS]: build CONFIG from the esphome checkout TREE, flash it over serial, capture SECONDS
# of log, then reset twice more for the boot heap line. Prints the RAM/flash lines and the harness summary lines.
set -u
TREE=$1; PORT=$2; CFG=$(cd "$(dirname "$3")" && pwd)/$(basename "$3"); SECS=${4:-120}
SKILL=$(cd "$(dirname "$0")/../.." && pwd); PY=${PY:-python3}; N=$(basename "$CFG" .yaml); OUT=$(dirname "$CFG")
(cd "$TREE" && PYTHONUNBUFFERED=1 PYTHONPATH=$TREE $PY -m esphome compile "$CFG" > "$OUT/$N-compile.log" 2>&1) || { echo "compile failed"; exit 1; }
(cd "$TREE" && PYTHONPATH=$TREE $PY -m esphome upload "$CFG" --device "$PORT" > "$OUT/$N-upload.log" 2>&1) || { echo "upload failed"; exit 1; }
$PY "$SKILL/scripts/serial_capture.py" "$PORT" "$SECS" "$OUT/run-$N.txt" --reset > /dev/null 2>&1
for b in 2 3; do $PY "$SKILL/scripts/serial_capture.py" "$PORT" 8 "$OUT/run-$N-boot$b.txt" --reset > /dev/null 2>&1; done
grep -a -E "RAM:|Flash:" "$OUT/$N-compile.log" | tail -2
grep -a -h -o "BOOTFREE [0-9]* TOTAL [0-9]*" "$OUT/run-$N.txt" "$OUT/run-$N-boot2.txt" "$OUT/run-$N-boot3.txt"
grep -a -E "canlb|mcp251xfd|CLKO" "$OUT/run-$N.txt" | grep -a -v "failed with error" | sed 's/^.*\]\[/[/' | cut -c1-200
echo "spi_device lines: $(grep -a -c spi_device "$OUT/run-$N.txt")  send failures: $(grep -a -c 'failed with error' "$OUT/run-$N.txt")"
