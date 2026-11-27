#!/bin/bash
# otaflow.sh NAME TREE DEVICE: OTA NAME (sim7080-atom.yaml unless YAML is set) from TREE to DEVICE (an address or
# NAME.local), then over the API: wait 20 s, modem.disable, modem.enable, wait 100 s and query CFUN and CPIN. The
# device log goes to NAME-api.txt, the states to NAME-states.txt. For a board whose serial port is not usable.
# env: PY, YAML, PIO_CORE
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D")
N=${1:?name}; T=${2:?tree}; FROM=${3:?device}
PY=${PY:-$T/venv/bin/python}; Y=${YAML:-$D/sim7080-atom.yaml}
TREE=$T PY=$PY "$S/pio_build.sh" compile "$N" "$Y" || exit 1
TREE=$T PY=$PY "$S/pio_build.sh" upload "$N" "$Y" "$FROM" || exit 1
sleep 10
H=$("$S/resolve.sh" "$N") || exit 1
timeout 300 $PY "$D/mdrive.py" "$H" "$D/$N-api.txt" wait:20 press:"Modem Disable" wait:15 press:"Modem Enable" \
  wait:100 press:"AT CFUN" wait:3 press:"AT CPIN" wait:3 > "$D/$N-states.txt" 2>&1
RC=$?
echo "$(date +%T) FLOWDONE rc=$RC"
exit $RC
