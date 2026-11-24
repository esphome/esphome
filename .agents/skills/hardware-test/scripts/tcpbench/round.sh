#!/bin/bash
# round.sh CFG TAG ROUND: flash CFG.yaml as <CFG><TAG> over serial from TREE, capture the console, run the bench set
# (TCP receive 20 s twice, TCP send 20 s, 10 x 512 KB bursts, 10 UDP bursts of 300) and append RESULT lines to
# results.txt next to this script. Alternate the trees per round (dev, PR, dev, PR) and summarise with summ.py.
# env: TREE (checkout to build), PORT (console serial port), PY (python with aioesphomeapi), SKILL (the skill's
#      scripts/ directory, for pio_build.sh, resolve.sh and serial_capture.py), PIO_CORE (optional)
H=$(cd "$(dirname "$0")" && pwd)
: "${TREE:?set TREE}" "${PORT:?set PORT}" "${SKILL:?set SKILL}"; PY=${PY:-python3}
c=${1:?cfg}; t=${2:?tag}; r=${3:?round}; N=$c$t
pkill -f "[s]erial_capture.py $PORT"; sleep 1
TREE=$TREE PY=$PY bash "$SKILL/pio_build.sh" compile "$N" "$H/$c.yaml" || exit 1
TREE=$TREE PY=$PY bash "$SKILL/pio_build.sh" upload "$N" "$H/$c.yaml" "$PORT" || exit 1
CAP=$H/cap-$N-r$r.txt
$PY "$SKILL/serial_capture.py" "$PORT" 420 "$CAP" > /dev/null 2>&1 &
IP=$(bash "$SKILL/resolve.sh" "$N" 40) || { echo "no join"; exit 1; }
wait_for() { for _ in $(seq "$2"); do grep -a -q -E "$1" "$CAP" && return 0; sleep 1; done; return 1; }
wait_for "BUILDTAG $N|Hostname: '$N'" 30 || { echo "wrong build on board"; exit 1; }
run() { echo "$N r$r $(date +%T) $* $($PY "$H/bench.py" "$IP" "$@" 2>&1 | tail -1)" | tee -a "$H/results.txt"; }
# 3 s gaps let the previous socket close and the rate lines settle before the next run
run rx 20; sleep 3; run rx 20; sleep 3; run tx 20; sleep 3; run tcpburst 10 512; sleep 3; run udpburst 10 300
# one more 10 s heap line so summ.py sees the low point after the last run
h=$(grep -a -c "heap free=" "$CAP"); for _ in $(seq 12); do [ "$(grep -a -c "heap free=" "$CAP")" -gt "$h" ] && break; sleep 1; done
pkill -f "[s]erial_capture.py $PORT"
echo "$N r$r caplines $(wc -l < "$CAP") WE_hosted $(grep -a -c -E 'E \([0-9]+\)|W \([0-9]+\)' "$CAP")" | tee -a "$H/results.txt"
