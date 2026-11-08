#!/bin/bash
# run.sh NAME SECONDS [apiclient.py options]: reset the already flashed C6 and capture SECONDS into out-NAME/cap.txt
# while apiclient.py holds an API connection (optionally sending light transitions), then print summ.py's totals.
# Build and flash first with: esphome -s name NAME -s ps light|none run --no-logs common.yaml
# env: TREE (esphome checkout), PORT (serial port), SC (path to serial_capture.py), PY (python with aioesphomeapi)
R=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; S=${2:?seconds}; shift 2
# the API client holds SECONDS - 25 (boot and Wi-Fi take up to 25 s)
[ "$S" -ge 40 ] || { echo "SECONDS must be at least 40"; exit 1; }
T=${TREE:?set TREE}; P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}
OUT=$R/out-$N; rm -rf "$OUT"; mkdir -p "$OUT"
pkill -f "[s]erial_capture.py $P"
$PY "$SC" "$P" "$S" "$OUT/cap.txt" --reset > /dev/null 2>&1 < /dev/null & CAP=$!
for _ in $(seq 1 60); do
  IP=$(grep -a -o "IP Address: [0-9.]*" "$OUT/cap.txt" 2>/dev/null | head -1 | awk '{print $3}')
  [ -n "$IP" ] && break; sleep 1
done
[ -n "$IP" ] || { echo "no IP in the capture"; kill $CAP; exit 1; }
$PY "$R/apiclient.py" "$IP" $((S - 25)) "$@" > "$OUT/api.txt" 2>&1
API_RC=$?
wait $CAP
tail -1 "$OUT/api.txt"
$PY "$R/summ.py" "$OUT/cap.txt"
SUMM_RC=$?
[ $API_RC = 0 ] || { echo "FAIL: API driver exited $API_RC"; exit 1; }
[ $SUMM_RC = 0 ] || exit 1
