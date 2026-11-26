#!/bin/bash
# run.sh NAME PORT [STEPS_FILE]: serial capture with a reset, then the prelude and steps of STEPS_FILE (default
# steps.sh here, which sets the PRE and STEPS arrays) through drive.py; writes NAME-ser.txt, NAME-pre.txt and
# NAME-drive.txt. env: PY, BAUD (default 115200; use 921600 for long climate frames), STEP_S (default 2.5, the wait
# after each step; the prelude always uses 2.5 s since it is not compared)
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); N=${1:?name}; P=${2:?port}; PY=${PY:-python3}
# shellcheck source=/dev/null
source "${3:-$D/steps.sh}"
cd "$D" || exit 1
pkill -f "[s]erial_capture.py $P "; sleep 1
# a stale capture would feed the IP lookup below
rm -f "$D/$N-ser.txt"
nohup $PY "$S/serial_capture.py" "$P" 1000 "$D/$N-ser.txt" --reset --baud "${BAUD:-115200}" > /dev/null 2>&1 &
IP=
for _ in $(seq 1 60); do
  IP=$(LC_ALL=C grep -a -o -E "IP Address: [0-9.]+" "$N-ser.txt" 2>/dev/null | tail -1 | cut -d' ' -f3)
  [ -n "$IP" ] && break; sleep 2
done
[ -z "$IP" ] && { echo "no IP in $N-ser.txt"; pkill -f "[s]erial_capture.py $P "; exit 1; }
sleep 3
STEP_S=2.5 $PY drive.py "$IP" "${PRE[@]}" > "$N-pre.txt" 2>&1; PRC=$?
STEP_S=${STEP_S:-2.5} $PY drive.py "$IP" "${STEPS[@]}" > "$N-drive.txt" 2>&1; DRC=$?
sleep 2; pkill -f "[s]erial_capture.py $P "; sleep 1
echo "$N steps=$(grep -c STEP "$N-drive.txt") raw=$(LC_ALL=C grep -a -c 'Received Raw' "$N-ser.txt")" \
  "boots=$(LC_ALL=C grep -a -c 'Running through setup\|rst cause' "$N-ser.txt")"
# a failed drive leaves a partial run: fail so frames.py and compare.py are not run on it
if [ $PRC -ne 0 ] || [ $DRC -ne 0 ]; then
  echo "$N drive failed: prelude rc=$PRC steps rc=$DRC (see $N-pre.txt, $N-drive.txt)"
  exit 1
fi
