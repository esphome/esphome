#!/bin/bash
# runcopy.sh NAME YAML: writes to a copy number and its source, reads of both (c6-copy.yaml). env: see common.sh
. "$(dirname "$0")/common.sh"; N=${1:?name}; Y=${2:?yaml}
join_fresh $N $Y 400
read -r C R < <($D epbydesc "$N boot interrogate" NumCopy NumR); echo "copy $C numr $R"; need "$C" "$R"
$D mark "$N copy writes copy ep $C numr ep $R"
wf $C 12; rd $R 0x000d
wf $R 7; rd $C 0x000d
$D mark "$N done"
