#!/bin/bash
# verdict.sh CASE...: what TREE's validation says about each case without a board: config errors, RMT memory
# warnings, and which of the case's components were registered in the generated main.cpp.
# env: TREE (esphome checkout), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
[ $# -gt 0 ] || { echo "usage: verdict.sh CASE..."; exit 1; }
for c in "$@"; do
  case $c in s3*) b=s3;; c6*) b=c6;; *) echo "$c: unknown board"; exit 1;; esac
  G=$R/gen/$c; mkdir -p "$G"
  python3 "$R/gen_case.py" "$c" > "$G/rmt$b.yaml" || exit 1
  (cd "$T" && $PY -m esphome compile --only-generate "$G/rmt$b.yaml" > "$G/gen.log" 2>&1); rc=$?
  w=$(grep -E "WARNING RMT memory" "$G/gen.log" | sed -E "s/.*, ('[^']*') (will not fit|may not fit).*/\1 \2/" | head -1)
  e=$(grep -E "each need one|Must be (an even|at most)" "$G/gen.log" | sed -E 's/^ *//' | cut -c1-70 | head -1)
  reg=$(grep -o "register_component_([a-z0-9]*" "$G/.esphome/build/rmt$b/src/main.cpp" 2>/dev/null | sed 's/register_component_(//' | grep -E "^(so|tx|rx)" | tr '\n' ' ')
  echo "$c rc=$rc warn=[${w:-none}] err=[${e:-none}] reg=[$reg]"
done
