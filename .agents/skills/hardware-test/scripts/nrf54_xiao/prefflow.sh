#!/bin/bash
# prefflow.sh NAME PORT: build pref.yaml as NAME and flash it (pyOCD) through pio_build.sh, capture the first boot and
# its soft reboot (100 s), then 40 s around a probe reset with no clean shutdown. Events go to NAME-events.txt.
# env: TREE, PY, PIO_CORE (as pio_build.sh), PROBE_ID (pyOCD unique id of the XIAO's probe), PYOCD_PY (a Python with
# pyOCD, such as ESPHome's sdk-nrf penv). Exits 1 when the build, the flash or a capture fails.
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); N=${1:?name}; P=${2:?port}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; ID=${PROBE_ID:?set PROBE_ID}; OCD=${PYOCD_PY:?set PYOCD_PY}
bash "$S/pio_build.sh" compile "$N" "$D/pref.yaml" || exit 1
$PY "$S/serial_capture.py" "$P" 100 "$D/$N-cap.txt" > /dev/null 2> "$D/$N-cap.err" &
CAP=$!
sleep 2
bash "$S/pio_build.sh" upload "$N" "$D/pref.yaml" "$P" || { kill $CAP 2>/dev/null; exit 1; }
wait $CAP
grep -a -q "SOFT REBOOT" "$D/$N-cap.txt" || { echo "FAIL: no soft reboot in $D/$N-cap.txt"; exit 1; }
$PY "$S/serial_capture.py" "$P" 40 "$D/$N-rst-cap.txt" > /dev/null 2>&1 &
CAP=$!
sleep 12
echo "probe reset at $(date +%T)"
$OCD -m pyocd reset -t nrf54lm20a -u "$ID" -m sw > /dev/null 2>&1 ||  # -m hw does nothing on this probe
  { kill $CAP 2>/dev/null; echo "FAIL: probe reset"; exit 1; }
wait $CAP || { echo "FAIL: no capture after the probe reset"; exit 1; }
grep -a -q "BOOT build" "$D/$N-rst-cap.txt" || { echo "FAIL: no boot after the probe reset"; exit 1; }
cat "$D/$N-cap.txt" "$D/$N-rst-cap.txt" |
  grep -a -E "Log initialized|compiled on|BOOT build|SOFT REBOOT|counter=|preferences|Loaded|settings|rror" |
  grep -a -v "save in flash" > "$D/$N-events.txt"
grep -a "BOOT build" "$D/$N-events.txt"
