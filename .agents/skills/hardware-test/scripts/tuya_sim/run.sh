#!/bin/bash
# run.sh NAME TREE: flash NAME (already compiled from TREE with pio_build.sh), capture the serial log with a reset,
# and drive the scenario over the API. Outputs NAME-log.txt, NAME-states.json, NAME-drive.txt.
# Needs secrets.yaml (wifi_ssid, wifi_password, api_key) next to tuya.yaml.
# env: PORT (the board), API_KEY, PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); N=${1:?name}; T=${2:?tree}; P=${PORT:?set PORT}
PY=${PY:-$T/venv/bin/python}
pkill -f "[s]erial_capture.py $P "
# outputs of an earlier run must not pass for this one in compare.sh
rm -f "$D/$N-log.txt" "$D/$N-states.json" "$D/$N-drive.txt"
TREE=$T PY=$PY "$S/pio_build.sh" upload "$N" "$D/tuya.yaml" "$P" || exit 1
# 130 s is an upper bound; the capture is stopped right after the drive
$PY "$S/serial_capture.py" "$P" 130 "$D/$N-log.txt" --reset &
CAP=$!
IP=$("$S/resolve.sh" "$N" 40) || { kill $CAP; exit 1; }
PYTHONUNBUFFERED=1 timeout 120 $PY "$D/tuyadrive.py" "$IP" "$D/$N-states.json" > "$D/$N-drive.txt" 2>&1
RC=$?
echo "drive rc=$RC"
sleep 2; kill $CAP; wait $CAP 2>/dev/null
grep -a -c "SIM RX" "$D/$N-log.txt"
exit $RC
