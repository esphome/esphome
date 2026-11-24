#!/bin/bash
# run.sh NAME TREE METHOD SECS [HAMMER]: build and flash acd.yaml as NAME from TREE with METHOD (leading_pulse, leading
# or trailing), capture SECS of serial log from a reset into NAME-cap.txt. HAMMER=1 also sends back to back HTTP
# requests (two at a time) to the web server from 250 s to 430 s after boot and pings it; results in NAME-hammer.txt.
# The sweep needs about 250 s; summarise with `ana.py NAME-cap.txt` and `agg.py *-cap.txt`.
# env: PORT, PY, PIO_CORE
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D")
N=${1:?name}; T=${2:?tree}; M=${3:?method}; SECS=${4:?seconds}; H=${5:-0}
P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}
case $M in
  leading_pulse) MO=(-s method "leading pulse" -s method_name lp -s method_idx 0) ;;
  leading) MO=(-s method leading -s method_name ld -s method_idx 1) ;;
  trailing) MO=(-s method trailing -s method_name tr -s method_idx 2) ;;
  *) echo "unknown method $M"; exit 2 ;;
esac
pkill -f "[s]erial_capture.py $P "
TREE=$T PY=$PY "$S/pio_build.sh" upload "$N" "$D/acd.yaml" "$P" "${MO[@]}" || exit 1
$PY "$S/serial_capture.py" "$P" "$SECS" "$D/$N-cap.txt" --reset &
CP=$!
if [ "$H" = 1 ]; then
  sleep 250
  if IP=$("$S/resolve.sh" "$N" 10); then
    end=$((SECONDS + 180))
    while [ $SECONDS -lt $end ]; do
      pids=""
      for _ in 1 2; do
        curl -s -m 20 -o /dev/null -w '%{http_code} %{time_total}\n' "http://$IP/" &
        pids="$pids $!"
      done
      wait $pids
    done > "$D/$N-hammer.txt" 2>&1
    ping -c 30 -i 0.3 "$IP" | tail -1 > "$D/$N-ping.txt"
    echo "hammer ok=$(grep -c '^200' "$D/$N-hammer.txt") other=$(grep -vc '^200' "$D/$N-hammer.txt")" \
      "ping $(cat "$D/$N-ping.txt")"
  else
    echo "no address for $N, hammer skipped"
  fi
fi
wait $CP
echo "capture done $N"
