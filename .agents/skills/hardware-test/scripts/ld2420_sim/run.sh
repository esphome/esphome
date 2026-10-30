#!/bin/bash
# run.sh LABEL MODE HOST: build ld<MODE><LABEL> (MODE energy or simple) for a D1 mini from TREE, flash it by USB, wait
# for the API at HOST (the serial console is the looped back bus) and record two lddrive.py runs. Then compare the
# same run of two builds: compare.py out-ldenergydev/run1.txt out-ldenergypr/run1.txt
# env: TREE (esphome checkout), PORT (serial port), PY (checkout python with aioesphomeapi)
R=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; M=${2:?energy or simple}; H=${3:?device address}
T=${TREE:?set TREE}; P=${PORT:?set PORT}; PY=${PY:-$T/venv/bin/python}
case $M in energy) FW=v1.5.5; B="Energy steps";; simple) FW=v1.5.3; B="Simple steps";; *) echo "MODE is energy or simple"; exit 2;; esac
NAME=ld$M$L; OUT=$R/out-$NAME; rm -rf "$OUT"; mkdir -p "$OUT"
printf 'substitutions:\n  fw: "%s"\nesphome:\n  name: %s\n  includes:\n    - ldsim.hpp\npackages:\n  ld: !include ld2420-sim-common.yaml\n' "$FW" "$NAME" > "$R/$NAME.yaml"
(cd "$T" && PYTHONPATH=$T $PY -m esphome run --no-logs "$R/$NAME.yaml" --device "$P" > "$OUT/upload.log" 2>&1) || { echo "build or upload failed"; exit 1; }
grep -a -E '^(RAM|Flash):' "$OUT/upload.log"
for r in 1 2; do
  ok=0
  for _ in $(seq 30); do sleep 3; timeout 60 $PY "$R/lddrive.py" "$H" "$B" "$OUT/run$r.txt" 2>/dev/null && { ok=1; break; }; done
  [ $ok = 1 ] || { echo "FAIL: no API connection for run $r"; exit 1; }
  echo "run $r: $(grep -c ' STATE ' "$OUT/run$r.txt") states, $(grep -a -o 'cmds [0-9]* acks [0-9]*' "$OUT/run$r.txt")"
done
