#!/bin/bash
# build.sh NAME TREE: compile NAME.yaml (next to this script) with the esphome package from TREE.
# env: PY (python with esphome's dependencies, default TREE/venv/bin/python)
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; T=${2:?tree}; PY=${PY:-$T/venv/bin/python}
cd "$D" && PYTHONPATH=$T $PY -c 'import esphome; print(esphome.__file__)' > "$D/$N-compile.log" || { echo "no esphome in $T"; exit 1; }
PYTHONPATH=$T PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome compile "$D/$N.yaml" >> "$D/$N-compile.log" 2>&1 ||
  { echo "build failed, see $D/$N-compile.log"; exit 1; }
grep -a -E '(RAM|Flash):' "$D/$N-compile.log"
