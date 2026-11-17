#!/bin/bash
# rgrun.sh E32V D1V: flash rg32<E32V>.yaml and rg66<D1V>.yaml (p = PR_TREE, b = BASE_TREE), capture 150 s on both
# and print the last STAT lines, the packet_transport status and the warning count. Make the files from
# rg32.example.yaml (rg66 is the same with esp8266 and the peer swapped).
# env: PR_TREE, BASE_TREE, PORT_D1, PORT_E32, PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); cd "$D" || exit 1
E=${1:?esp32 build}; B=${2:?d1 build}; SECS=150
tree() { if [ "$1" = p ]; then echo "${PR_TREE:?set PR_TREE}"; else echo "${BASE_TREE:?set BASE_TREE}"; fi; }
P1=${PORT_D1:?set PORT_D1}; P2=${PORT_E32:?set PORT_E32}; PY=${PY:-$(tree p)/venv/bin/python}
TREE=$(tree "$B") bash "$S/pio_build.sh" upload "rg66$B" "rg66$B.yaml" "$P1" || exit 1
TREE=$(tree "$E") bash "$S/pio_build.sh" upload "rg32$E" "rg32$E.yaml" "$P2" || exit 1
$PY "$S/serial_capture.py" "$P1" $SECS "rg-$E$B-d1.log" --reset > /dev/null 2>&1 &
$PY "$S/serial_capture.py" "$P2" $SECS "rg-$E$B-e32.log" --reset > /dev/null 2>&1 &
wait
for b in e32 d1; do
  F=rg-$E$B-$b.log
  echo "e32=$E d1=$B $b: $(grep -a 'STAT rg' "$F" | tail -1 | sed 's/.*STAT rg //')" \
    "status=$(grep -a PTSTATUS "$F" | awk '{print $NF}' | tr -d '\n') W=$(grep -a -c '\[W\]\|\[E\]' "$F")"
done
