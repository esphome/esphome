#!/bin/bash
# suite.sh BUILD TREE: flash spBUILD.yaml (a copy of example-run.yaml) from TREE, capture the board, run every case
# (CASES="a b" picks some), print the client RESULT and the simulator's SIMDONE totals per case.
# env: PORT (the board's console), PY, PIO_CORE (default ./pio)
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); cd "$D" || exit 1
B=${1:?build}; TREE=${2:?tree}; PORT=${PORT:?set PORT}; PY=${PY:-$TREE/venv/bin/python}
pkill -f "[s]erial_capture.py $PORT "; sleep 1
TREE=$TREE PY=$PY bash "$S/pio_build.sh" upload "sp$B" "$D/sp$B.yaml" "$PORT" || exit 1
CAP=$D/$B-suite-cap-$(date +%H%M%S).txt
nohup $PY "$S/serial_capture.py" "$PORT" 7200 "$CAP" > /dev/null 2>&1 &
IP=$(bash "$S/resolve.sh" "sp$B" 40) || exit 1
sleep 10
grep -a HEAP $CAP | tail -1 | sed 's/^/BOOT /'
run() {  # run LABEL CLIENT-ARGS...
  local L=$1; shift
  [[ ${CASES:-all} == all || " $CASES " == *" $L "* ]] || return
  local N
  N=$(( $(wc -l < "$CAP") ))
  timeout 1200 $PY spclient.py $IP "$@" > $B-$L.txt 2>&1
  sleep 2
  echo "== $L: $(grep RESULT $B-$L.txt)"
  echo "   $(grep -a SIMDONE $CAP | tail -1 | sed 's/.*SIMDONE//')"
  echo "   $(tail -n +$N $CAP | grep -a -E 'SIMSTAT|ASHWIN' | sed 's/.*backlog_max=\([0-9]*\).*/\1/' | sort -n | tail -1 | sed 's/^/peak backlog /')"
  grep -a -c -E "CONNECTION CLOSED|DISCONNECT" $B-$L.txt | sed 's/^/   disconnects /'
}
run normal1920 --baud 1920 --mode reply --interval 600 --duration 180
run normal115200 --baud 115200 --mode stream --duration 120
run pause1920 --baud 1920 --mode reply --interval 600 --pauses 12 --pause 10 --read 15
run pause115200 --baud 115200 --mode reply --interval 20 --pauses 12 --pause 5 --read 8
run tap115200 --baud 115200 --mode ash --protocol --pauses 10 --pause 5 --read 8
run normal115200r --baud 115200 --mode reply --interval 20 --duration 120
sleep 11
grep -a HEAP $CAP | tail -1 | sed 's/^/END /'
grep -a -c -E "rst:|Guru|panic|abort|Backtrace" $CAP | sed 's/^/crash lines /'
