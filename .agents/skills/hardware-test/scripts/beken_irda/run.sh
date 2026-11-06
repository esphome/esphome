#!/bin/bash
# run.sh NAME [SECONDS]: build NAME.yaml from TREE, flash the BK72xx over serial with flash_bk72xx.py, capture SECONDS
# (default 120) of its log to NAME-serial.txt (opening the port resets the chip), then score it with ana.py.
# Run it in a copy of templates/beken_irda. env: TREE (esphome checkout), PORT (serial port), SC (path to
# serial_capture.py), PY (checkout python), BKPY + BKFLASH (python and flash_bk72xx.py)
H=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; S=${2:-120}; T=${TREE:?set TREE}; P=${PORT:?set PORT}; SC=${SC:?set SC}
PY=${PY:-$T/venv/bin/python}
(PYTHONPATH=$T PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome compile "$N.yaml" > "$N-compile.log" 2>&1) || { echo "build failed"; exit 1; }
${BKPY:?set BKPY} "${BKFLASH:?set BKFLASH}" flash write -d "$P" ".esphome/build/$N/.pioenvs/$N/firmware.uf2" > "$N-flash.log" 2>&1 ||
  { echo "flash failed (it can fail several times in a row; retry, or use runota.sh once the board is on Wi-Fi)"; tail -5 "$N-flash.log"; exit 1; }
$PY "$SC" "$P" "$S" "$N-serial.txt" > /dev/null 2>&1 || { echo "serial capture failed"; exit 1; }
echo "$N captured $(wc -l < "$N-serial.txt") lines"
$PY "$H/ana.py" "$N-serial.txt"
