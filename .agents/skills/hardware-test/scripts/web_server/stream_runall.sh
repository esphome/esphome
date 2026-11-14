#!/bin/bash
# stream_runall.sh NAME PORT: upload stream-dut.yaml as NAME over serial PORT, capture serial for 15 min, then run every
# stream_retest.py mode (head saved to head-NAME.bin, multi, stall, churn, wifidrop) and print the stall closes, the
# heap and any reset or crash lines. Run it for both builds and compare the heads with head_compare.py.
# env: TREE, PY (secrets.yaml next to stream-dut.yaml)
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); N=${1:?name}; P=${2:?serial port}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; R="$PY -I $D/stream_retest.py"
pkill -f "[s]erial_capture.py $P "; sleep 1
"$S/pio_build.sh" upload "$N" "$D/stream-dut.yaml" "$P" || exit 1
L=$D/cap-$N.txt
$PY "$S/serial_capture.py" "$P" 900 "$L" --reset > /dev/null 2>&1 &
CAP=$!; trap 'kill $CAP 2>/dev/null' EXIT
n=0; until grep -a -q "IP Address" "$L" 2>/dev/null || [ $n -gt 40 ]; do sleep 2; n=$((n + 1)); done
IP=$(grep -a "IP Address" "$L" | head -1 | awk '{print $NF}')
[ -n "$IP" ] || { echo "no IP Address line in $L"; exit 1; }
sleep 5; grep -a heap "$L" | tail -1 | cut -c1-120
$R "$IP" head --out "$D/head-$N.bin"
$R "$IP" multi --runs 3
$R "$IP" stall --runs 3
grep -a "Closing stuck" "$L" | cut -c1-160
$R "$IP" churn
$R "$IP" wifidrop
sleep 5; grep -a heap "$L" | tail -1 | cut -c1-120
grep -a -E "rst:|ESPHome version|Guru|abort|Backtrace|wifi drop|connection closed \(" "$L" | cut -c1-140 | tail -15
