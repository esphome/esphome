#!/bin/bash
# runnumber.sh NAME YAML: fresh join, number descriptor reads, writes in and out of range, switch write, then a reset
# and reads after it to check restore (c6-number.yaml, #23051). env: TREE, PY, PORT, COORD_IEEE
Z=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; Y=${2:?yaml}; D="${PY:-python3} $Z/zbdrv.py"
: "${COORD_IEEE:?set COORD_IEEE (address -x on the coordinator)}"
$D send "network open -t 254"
since=$($D lines); $D mark "$N start"
CHIP=${CHIP-esp32c6} $Z/flashdev.sh $N $Y 900 || exit 1
short=$($D waitjoin $since 120); echo "joined $short"; [ "$short" = NOJOIN ] && exit 2
sleep 8
$D interrogate "$N boot" $short
NUM=$($D epbydesc "$N boot interrogate" Num); NUMR=$($D epbydesc "$N boot interrogate" NumR); SW=$($D epbydesc "$N boot interrogate" Sw)
echo "num $NUM numr $NUMR sw $SW"
$D mark "$N writes num ep $NUM numr ep $NUMR sw ep $SW"
$D send "zdo bind -c 0x000d -S $NUM -D 1 -s $short -d $COORD_IEEE" 3
$D send "zcl send_gen config_rp -d $short --dst-ep $NUM -e 1 -c 0x000d -a 0x0055 -t 0x39 --min 1 --max 60 --delta 0x00000000" 3
$D send "zcl send_gen write -d $short --dst-ep $NUM -e 1 -c 0x000d -a 0x0055 -t 0x39 -v 0x00002842" 4   # 42.0
$D send "zcl send_gen read -d $short --dst-ep $NUM -e 1 -c 0x000d -a 0x0055" 2
$D send "zcl send_gen write -d $short --dst-ep $NUMR -e 1 -c 0x000d -a 0x0055 -t 0x39 -v 0x0000b041" 4  # 22.0
$D send "zcl send_gen read -d $short --dst-ep $NUMR -e 1 -c 0x000d -a 0x0055" 2
$D send "zcl send_gen write -d $short --dst-ep $NUMR -e 1 -c 0x000d -a 0x0055 -t 0x39 -v 0x00001643" 4  # 150.0 out of range
$D send "zcl send_gen read -d $short --dst-ep $NUMR -e 1 -c 0x000d -a 0x0055" 2
$D send "zcl send_gen write -d $short --dst-ep $SW -e 1 -c 0x0010 -a 0x0055 -t 0x10 -v 0x01" 4
$D send "zcl send_gen read -d $short --dst-ep $SW -e 1 -c 0x0010 -a 0x0055" 2
$D mark "$N waiting for preference save"
sleep 15
$D mark "$N reset device"
pkill -f "[s]erial_capture.py $PORT"; sleep 1
${PY:-python3} $Z/../serial_capture.py $PORT 300 $Z/cap-$N-r.txt --reset > /dev/null 2>&1 &
sleep 30
$D mark "$N after reset reads"
for e in $NUM $NUMR; do $D send "zcl send_gen read -d $short --dst-ep $e -e 1 -c 0x000d -a 0x0055" 2; done
$D send "zcl send_gen read -d $short --dst-ep $SW -e 1 -c 0x0010 -a 0x0055" 2
$D mark "$N done"
