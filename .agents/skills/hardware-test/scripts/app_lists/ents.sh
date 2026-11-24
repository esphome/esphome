#!/bin/bash
# ents.sh CAPTURE: the dump_config lines of a serial capture that name an entity, without timestamps or line numbers,
# sorted, for a diff between builds.
grep -a -E "\[C\]\[[a-z_.]+:[0-9]+\]: .*'[^']+'" "$1" | sed -E 's/^\[[0-9:.]+\]//; s/:[0-9]+\]/]/' | sort
