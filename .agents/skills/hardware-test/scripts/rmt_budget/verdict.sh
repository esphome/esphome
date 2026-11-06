#!/bin/bash
# verdict.sh CASE...: what TREE's validation says about each case without a board: config errors, RMT memory
# warnings, and which of the case's components were registered in the generated main.cpp.
# env: TREE (esphome checkout), PY (checkout python)
R=$(cd "$(dirname "$0")" && pwd); T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
[ $# -gt 0 ] || { echo "usage: verdict.sh CASE..."; exit 1; }
for c in "$@"; do
  b=$(awk -v c="$c" '$1 == c {print $2}' "$R/cases.txt"); [ -n "$b" ] || { echo "$c: not in cases.txt"; exit 1; }
  G=$R/gen/$c; mkdir -p "$G"
  python3 "$R/gen_case.py" "$c" > "$G/rmt$b.yaml" || exit 1
done
one() {
  c=$1; G=$R/gen/$c; y=$(ls "$G"/rmt*.yaml); b=$(basename "$y" .yaml)
  (cd "$T" && $PY -m esphome compile --only-generate "$y" > "$G/gen.log" 2>&1); rc=$?
  w=$(grep -E "WARNING RMT memory" "$G/gen.log" | sed -E "s/.*these components, (.*) will not fit.*/\1 will not fit/" | head -1)
  e=$(grep -E "each need one|Must be (an even|at most)" "$G/gen.log" | sed -E 's/^ *//' | cut -c1-70 | head -1)
  reg=$(grep -o "register_component_([a-z0-9]*" "$G/.esphome/build/$b/src/main.cpp" 2>/dev/null | sed 's/register_component_(//' | grep -E "^(so|tx|rx)" | tr '\n' ' ')
  echo "$c rc=$rc warn=[${w:-none}] err=[${e:-none}] reg=[$reg]" > "$G/verdict.txt"
}
export -f one; export R T PY
printf '%s\n' "$@" | xargs -P 6 -I{} bash -c 'one "$1"' _ {}
for c in "$@"; do cat "$R/gen/$c/verdict.txt"; done
