#!/bin/bash
# runcopy.sh NAME YAML: writes to a copy number and its source, reads of both (c6-number.yaml plus numcopy.yaml).
# env: TREE, PY, PORT
Z=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; Y=${2:?yaml}; D="${PY:-python3} $Z/zbdrv.py"
since=$($D lines); $D mark "$N start"
CHIP=${CHIP-esp32c6} $Z/flashdev.sh $N $Y 400 || exit 1
short=$($D waitjoin $since 120); echo "joined $short"; [ "$short" = NOJOIN ] && exit 2
sleep 8
$D interrogate "$N boot" $short
C=$($D epbydesc "$N boot interrogate" NumCopy); R=$($D epbydesc "$N boot interrogate" NumR); echo "copy $C numr $R"
$D mark "$N copy writes copy ep $C numr ep $R"
$D send "zcl send_gen write -d $short --dst-ep $C -e 1 -c 0x000d -a 0x0055 -t 0x39 -v 0x00004041" 4  # 12.0
$D send "zcl send_gen read -d $short --dst-ep $C -e 1 -c 0x000d -a 0x0055" 2
$D send "zcl send_gen read -d $short --dst-ep $R -e 1 -c 0x000d -a 0x0055" 2
$D send "zcl send_gen write -d $short --dst-ep $R -e 1 -c 0x000d -a 0x0055 -t 0x39 -v 0x0000e040" 4  # 7.0
$D send "zcl send_gen read -d $short --dst-ep $C -e 1 -c 0x000d -a 0x0055" 2
$D send "zcl send_gen read -d $short --dst-ep $R -e 1 -c 0x000d -a 0x0055" 2
$D mark "$N done"
