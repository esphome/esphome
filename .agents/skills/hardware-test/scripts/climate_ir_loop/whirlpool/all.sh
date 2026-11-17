#!/bin/bash
# all.sh PORT BASE_TREE PR_TREE: per whirlpool model (DG11J1-3A, HITACHI_HCRA31NEWH, HISENSE_ARG), build ../base.yaml
# plus cl.yaml plus the D1 mini one-pin loopback from the base tree, then the PR tree, flash, run the whirlpool steps and split the frames. Base
# runs first per model, so the board ends on a PR build. Compare the hex lines with `grep -a "Sending:\|Received:"`.
# env: PY, BUILD_LOCK (optional)
W=$(cd "$(dirname "$0")" && pwd); L=$(dirname "$W"); S=$(dirname "$L"); P=${1:?port}
export PIO_CORE=${PIO_CORE:-$W/pio}
for mm in dg:DG11J1-3A hi:HITACHI_HCRA31NEWH hs:HISENSE_ARG; do
  m=${mm%%:*}
  for v in base pr; do
    if [ $v = base ]; then T=${2:?base tree}; else T=${3:?pr tree}; fi
    N=w$m$v
    printf 'substitutions:\n  name: %s\n  model: %s\npackages:\n  common: !include ../base.yaml\n  model: !include cl.yaml\n  board: !include ../esp8266-ir.yaml\n' \
      "$N" "${mm#*:}" > "$W/$N.yaml"
    echo "== $N"
    TREE=$T bash "$S/pio_build.sh" compile "$N" "$W/$N.yaml" || continue
    TREE=$T bash "$S/pio_build.sh" upload "$N" "$W/$N.yaml" "$P" || continue
    PY=${PY:-$T/venv/bin/python}
    (cd "$L" && STEP_S=4 PY=$PY bash run.sh "$N" "$P" "$W/steps.sh" && $PY frames.py "$N")
  done
done
echo DONE
