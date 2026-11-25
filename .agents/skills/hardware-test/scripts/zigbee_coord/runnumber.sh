#!/bin/bash
# runnumber.sh NAME YAML: fresh join, number descriptor reads, writes in and out of range, switch write, then a reset
# and reads after it to check restore (c6-number.yaml, #23051). env: see common.sh
. "$(dirname "$0")/common.sh"; N=${1:?name}; Y=${2:?yaml}
join_fresh $N $Y 900
read -r NUM NUMR SW < <($D epbydesc "$N boot interrogate" Num NumR Sw)
echo "num $NUM numr $NUMR sw $SW"; need "$NUM" "$NUMR" "$SW"
$D mark "$N writes num ep $NUM numr ep $NUMR sw ep $SW"
bindrp $NUM 0x000d 0x0055 0x39 "--min 1 --max 60 --delta 0x00000000"
wf $NUM 42; wf $NUMR 22; wf $NUMR 150  # 150 is out of range
wr $SW 0x0010 0x10 0x01; rd $SW 0x0010
$D mark "$N waiting for preference save"
sleep 15
$D mark "$N reset device"
NOFLASH=1 "$Z/flashdev.sh" $N-r $Y 300 || exit 1
sleep 30
grep -q "HWT boot done" "$Z/cap-$N-r.txt" || { echo "no boot line after the reset"; exit 1; }
$D mark "$N after reset reads"
rd $NUM 0x000d; rd $NUMR 0x000d; rd $SW 0x0010
$D mark "$N done"
