#!/bin/bash
# pairrun.sh PREFIX E32V D1V [SECS] [RUN]: flash <PREFIX>32<E32V>.yaml (ESP32) and <PREFIX>66<D1V>.yaml (D1 mini), each
# from the tree of its build letter (p = PR_TREE, anything else = BASE_TREE), capture both boards SECS seconds (default
# 150) from a reset into <PREFIX>-<E32V><D1V>[-RUN]-<board>.log and print a summary per board.
# PREFIX ur (urlink.yaml): last p2pf STAT and on_receive lines, sendto error count. PREFIX rg (rglink.yaml): last
# STAT rg line, packet_transport status, warning and error count. Make the files from ur32.example.yaml or
# rg32.example.yaml (the 66 file is the same with esp8266 and the peer swapped).
# env: PR_TREE, BASE_TREE, PORT_D1, PORT_E32, PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); cd "$D" || exit 1
X=${1:?prefix ur or rg}; E=${2:?esp32 build}; B=${3:?d1 build}; SECS=${4:-150}; L=$X-$E$B${5:+-$5}
tree() { if [ "$1" = p ]; then echo "${PR_TREE:?set PR_TREE}"; else echo "${BASE_TREE:?set BASE_TREE}"; fi; }
P1=${PORT_D1:?set PORT_D1}; P2=${PORT_E32:?set PORT_E32}; PY=${PY:-$(tree p)/venv/bin/python}
TREE=$(tree "$B") bash "$S/pio_build.sh" upload "${X}66$B" "${X}66$B.yaml" "$P1" || exit 1
TREE=$(tree "$E") bash "$S/pio_build.sh" upload "${X}32$E" "${X}32$E.yaml" "$P2" || exit 1
$PY "$S/serial_capture.py" "$P1" "$SECS" "$L-d1.log" --reset > /dev/null 2>&1 &
$PY "$S/serial_capture.py" "$P2" "$SECS" "$L-e32.log" --reset > /dev/null 2>&1 &
wait
for b in e32 d1; do
  F=$L-$b.log
  if [ "$X" = rg ]; then
    echo "e32=$E d1=$B $b: $(grep -a 'STAT rg' "$F" | tail -1 | sed 's/.*STAT rg //')" \
      "status=$(grep -a PTSTATUS "$F" | awk '{print $NF}' | tr -d '\n') W=$(grep -a -c '\[W\]\|\[E\]' "$F")"
  else
    echo "e32=$E d1=$B $b: $(grep -a 'STAT p2pf' "$F" | tail -1 | sed 's/.*STAT //')" \
      "$(grep -a 'on_receive' "$F" | tail -1 | sed 's/.*STAT //') senderr=$(grep -a -c 'sendto' "$F")"
  fi
done
