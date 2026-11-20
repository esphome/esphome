#!/bin/bash
# build.sh NAME TREE [LABEL]: compile NAME.yaml (next to this script) with the esphome package from TREE. With LABEL,
# first write NAME<LABEL>.yaml (its own device name, NAME.yaml as a package) and build that, so the builds of dev and
# the PR do not share a build directory.
# env: PY (python with esphome's dependencies, default TREE/venv/bin/python)
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; T=${2:?tree}; L=${3:-}; PY=${PY:-$T/venv/bin/python}
if [ -n "$L" ]; then
  printf 'esphome:\n  name: %s\npackages:\n  run: !include %s.yaml\n' "$N$L" "$N" > "$D/$N$L.yaml"; N=$N$L
fi
# Print which esphome package gets imported, so a missing or wrong TREE shows in the log.
cd "$D" && PYTHONPATH=$T $PY -c 'import esphome; print(esphome.__file__)' > "$D/$N-compile.log" || { echo "no esphome in $T"; exit 1; }
PYTHONPATH=$T PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome compile "$D/$N.yaml" >> "$D/$N-compile.log" 2>&1 ||
  { echo "build failed, see $D/$N-compile.log"; exit 1; }
grep -a -E '(RAM|Flash):' "$D/$N-compile.log"
echo "built $N"
