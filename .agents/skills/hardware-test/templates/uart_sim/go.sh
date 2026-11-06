#!/bin/bash
# usage: go.sh NAME TREE OPSFILE [SECONDS]: run.sh NAME TREE SECONDS with the ops read from OPSFILE, one per line.
# Copy it next to run.sh (see scripts/uart_sim/run.sh); PORT, SC and PY in the environment as for run.sh.
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; T=${2:?tree}; OPS=${3:?ops file}; S=${4:-150}
[ -x "$D/run.sh" ] || { echo "run.sh not found next to go.sh"; exit 1; }
[ -f "$D/$OPS" ] || { echo "ops file $D/$OPS not found"; exit 1; }
OPARR=(); while IFS= read -r l; do [ -n "$l" ] && OPARR+=("$l"); done < "$D/$OPS"
PYTHONPATH=$T exec "$D/run.sh" "$N" "$T" "$S" "${OPARR[@]}"
