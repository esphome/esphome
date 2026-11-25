#!/bin/bash
# flow.sh NAME TREE PORT [full|reboot]: flash NAME (sim7670-s3.yaml unless YAML is set) from TREE, capture the
# serial log to NAME.txt and wait for "modem connected" (PPP up).
#   full    (default) PPP traffic test, two warm reboots out of PPP, then modem.disable and modem.enable; each step
#           waits for the next "modem connected" (300 s each).
#   reboot  one warm reboot out of PPP.
# Driver logs go to NAME-*.txt. Exits 2 when PPP never comes up (or the serial capture dies first), 1 when a driver
# fails, a later step does not reconnect, or no "traffic status 2xx over PPP" line follows the traffic press.
# env: PY, YAML, PIO_CORE
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D")
N=${1:?name}; T=${2:?tree}; PORT=${3:?port}; MODE=${4:-full}
PY=${PY:-$T/venv/bin/python}; Y=${YAML:-$D/sim7670-s3.yaml}
pkill -f "[s]erial_capture.py $PORT "; sleep 1
TREE=$T PY=$PY "$S/pio_build.sh" compile "$N" "$Y" || exit 1
TREE=$T PY=$PY "$S/pio_build.sh" upload "$N" "$Y" "$PORT" || exit 1
CAP=$D/$N.txt
$PY "$S/serial_capture.py" "$PORT" 1800 "$CAP" &
CP=$!
cnt() { grep -a -c "modem connected" "$CAP"; }
waitc() {
  local want=$1 t=0
  until [ "$(cnt)" -ge "$want" ]; do
    kill -0 $CP 2>/dev/null || { echo "serial capture exited"; return 1; }
    sleep 2; t=$((t + 2))
    [ $t -ge "${2:-300}" ] && { echo "TIMEOUT waiting for connect $want"; return 1; }
  done
  echo "$(date +%T) connected #$want after ${t}s"
}
drive() { timeout "$1" $PY "$D/mdrive.py" "$H" "$D/$N-$2.txt" "${@:3}" > /dev/null; }
waitc 1 300 || { kill $CP; exit 2; }
sleep 8
H=$("$S/resolve.sh" "$N") || { kill $CP; exit 1; }
RC=0; REBOOTS=1
if [ "$MODE" = full ]; then
  REBOOTS=2
  drive 60 traffic press:"Traffic Test" wait:20 || { echo "traffic driver failed"; RC=1; }
  grep -a "traffic" "$CAP" | tail -3
  grep -aq 'traffic status 2[0-9][0-9] over PPP' "$CAP" || { echo "PPP traffic failed"; RC=1; }
fi
for i in $(seq 1 $REBOOTS); do
  c=$(cnt); echo "$(date +%T) reboot $i"; drive 30 "drv$i" press:Restart || { echo "reboot driver $i failed"; RC=1; continue; }
  waitc $((c + 1)) 300 || RC=1; sleep 15
done
if [ "$MODE" = full ]; then
  c=$(cnt); echo "$(date +%T) disable"
  if drive 60 drvd press:"Modem Disable" wait:25 press:"Modem Enable" wait:2; then
    waitc $((c + 1)) 300 || RC=1; sleep 5
  else
    echo "disable driver failed"; RC=1
  fi
fi
kill $CP; wait $CP 2>/dev/null
echo "$(date +%T) FLOWDONE rc=$RC"
exit $RC
