#!/bin/bash
# abrun.sh V RUN [SECS]: flash ur66V.yaml (D1 mini) and ur32V.yaml (ESP32) from the tree of build V (p = PR_TREE,
# anything else = BASE_TREE), capture both boards SECS seconds (default 150) from a reset and print the last STAT and
# on_receive lines and the sendto error count per board. Make urNNV.yaml from ur32.example.yaml (set name and peer).
# env: PR_TREE, BASE_TREE, PORT_D1, PORT_E32, PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); cd "$D" || exit 1
V=${1:?build}; R=${2:?run}; SECS=${3:-150}
if [ "$V" = p ]; then export TREE=${PR_TREE:?set PR_TREE}; else export TREE=${BASE_TREE:?set BASE_TREE}; fi
P1=${PORT_D1:?set PORT_D1}; P2=${PORT_E32:?set PORT_E32}; PY=${PY:-$TREE/venv/bin/python}
bash "$S/pio_build.sh" upload "ur66$V" "ur66$V.yaml" "$P1" || exit 1
bash "$S/pio_build.sh" upload "ur32$V" "ur32$V.yaml" "$P2" || exit 1
$PY "$S/serial_capture.py" "$P1" "$SECS" "ab$R-$V-d1.log" --reset > /dev/null 2>&1 &
$PY "$S/serial_capture.py" "$P2" "$SECS" "ab$R-$V-e32.log" --reset > /dev/null 2>&1 &
wait
for b in e32 d1; do
  F=ab$R-$V-$b.log
  echo "$V $R $b: $(grep -a 'STAT p2pf' "$F" | tail -1 | sed 's/.*STAT //')" \
    "$(grep -a 'on_receive' "$F" | tail -1 | sed 's/.*STAT //') senderr=$(grep -a -c 'sendto' "$F")"
done
