#!/bin/bash
# runone.sh NAME CONFIG PORT [SECS]: flash CONFIG as NAME with `esphome run --no-logs` (pyOCD through the XIAO's
# CMSIS-DAP probe) from TREE while a serial capture of SECS (default 240) stays open on the probe's PORT. The first
# seconds of the capture are the old firmware; cut at "Log initialized". A failed compile, flash or timeout stops the
# capture and exits 1. env: TREE, PY, PIO_CORE
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); N=${1:?name}; C=${2:?config}; P=${3:?port}; SECS=${4:-240}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
export PLATFORMIO_CORE_DIR=${PIO_CORE:-$D/pio} PYTHONPATH=$T PYTHONUNBUFFERED=1
$PY "$S/serial_capture.py" "$P" "$SECS" "$D/$N-cap.txt" > /dev/null 2> "$D/$N-cap.err" &
CAP=$!
sleep 2
if ! (cd "$T" && timeout 300 $PY -m esphome -s name "$N" run --no-logs "$D/$C" --device "$P") > "$D/$N-run.log" 2>&1
then
  # The capture holds the old firmware only; stop it so it is not read as a run of the new build.
  tail -1 "$D/$N-run.log"; kill $CAP 2>/dev/null; wait $CAP 2>/dev/null
  echo "FAIL: $N run failed (see $D/$N-run.log)"; exit 1
fi
tail -1 "$D/$N-run.log"
# A capture that ended during the flash holds only the old firmware
kill -0 $CAP 2>/dev/null || { echo "FAIL: the capture ended before the flash finished (see $D/$N-cap.err; if empty, raise SECS)"; exit 1; }
wait $CAP
