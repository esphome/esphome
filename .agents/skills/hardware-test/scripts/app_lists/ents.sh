#!/bin/bash
# ents.sh CAPTURE: the dump_config lines of a serial capture that name an entity, without timestamps or line numbers,
# sorted, for a diff between builds.
out=$(grep -a -E "\[C\]\[[a-z_.]+:[0-9]+\]: .*'[^']+'" "$1" | python3 "$(dirname "$0")/../lognorm.py" | sort)
[ -n "$out" ] || { echo "no entity dump_config lines in $1" >&2; exit 1; }
echo "$out"
