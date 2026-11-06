#!/bin/bash
# run.sh HOST TAG [ROUNDS]: va.py with reject,timeout alternating ROUNDS times (default 4) while the wake phrase plays
# near the device every 9 s; prints each run's mode and wake word audio length. Results in out-ww<TAG>/.
# env: PY (python with aioesphomeapi), API_KEY, WAKE_AUDIO (default wake.aiff), PLAY (default afplay)
D=$(cd "$(dirname "$0")" && pwd); H=${1:?host}; G=${2:?tag}; N=${3:-4}; PY=${PY:?set PY}; K=${API_KEY:?set API_KEY}
WAKE=${WAKE_AUDIO:-wake.aiff}; PLAY=${PLAY:-afplay}; O=$D/out-ww$G; rm -rf "$O"; mkdir -p "$O"
[ -f "$WAKE" ] || { echo "FAIL: no $WAKE (see the recipe)"; exit 1; }
M=$(for _ in $(seq "$N"); do printf 'reject,timeout,'; done); M=${M%,}
(timeout 300 $PY "$D/va.py" "$H" "$K" "$O" "$M" > "$O/va.log" 2>&1; echo $? > "$O/rc"; touch "$O/done") &
sleep 9; i=0
until [ -e "$O/done" ] || [ $i -ge $((N * 2 + 17)) ]; do $PLAY "$WAKE"; i=$((i + 1)); sleep 9; done
wait
grep SUMMARY "$O/va.log" | awk '{print $3, $4, "ms"}'
echo "rejected logs: $(grep -c 'Wake word rejected' "$O/va.log")  timeout logs: $(grep -c 'was not accepted' "$O/va.log")"
[ "$(cat "$O/rc")" = 0 ] || { echo "FAIL: va.py exited $(cat "$O/rc"): $(grep -a 'FAIL' "$O/va.log" | tail -1)"; exit 1; }
for m in reject timeout; do
  [ "$(grep -c "RESULT .*\"mode\": \"$m\"" "$O/va.log")" -ge "$N" ] || { echo "FAIL: fewer than $N $m runs"; exit 1; }
done
[ "$(grep -c 'Wake word rejected' "$O/va.log")" -ge "$N" ] && [ "$(grep -c 'was not accepted' "$O/va.log")" -ge "$N" ] ||
  { echo "FAIL: the device did not log every reject or timeout verdict"; exit 1; }
echo PASS
