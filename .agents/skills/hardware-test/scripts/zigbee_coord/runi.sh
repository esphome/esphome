#!/bin/bash
# runi.sh NAME YAML: fresh join, interrogate, bind and configure reporting, switch and number writes, 75 s of reports,
# final reads (c6-ents.yaml or nrf-ents.yaml). CHIP= (empty) for the nRF dongle. env: TREE, PY, PORT, COORD_IEEE
Z=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; Y=${2:?yaml}; D="${PY:-python3} $Z/zbdrv.py"
: "${COORD_IEEE:?set COORD_IEEE (address -x on the coordinator)}"
since=$($D lines); $D mark "$N start"
CHIP=${CHIP-esp32c6} $Z/flashdev.sh $N $Y 400 || exit 1
short=$($D waitjoin $since 150); echo "joined $short"; [ "$short" = NOJOIN ] && exit 2
sleep 6
$D interrogate "$N boot" $short | tee $Z/int-$N.txt
eps_with() { grep "'$1'" $Z/int-$N.txt | awk '{print $2}'; }
$D mark "$N bind and configure reporting"
for e in $(eps_with 0xc); do $D send "zdo bind -c 0x000c -S $e -D 1 -s $short -d $COORD_IEEE" 3; $D send "zcl send_gen config_rp -d $short --dst-ep $e -e 1 -c 0x000c -a 0x0055 -t 0x39 --min 1 --max 30 --delta 0x00000000" 3; done
for e in $(eps_with 0xf); do $D send "zdo bind -c 0x000f -S $e -D 1 -s $short -d $COORD_IEEE" 3; $D send "zcl send_gen config_rp -d $short --dst-ep $e -e 1 -c 0x000f -a 0x0055 -t 0x10 --min 1 --max 30" 3; done
SW=$($D epbydesc "$N boot interrogate" Sw); NUM=$($D epbydesc "$N boot interrogate" Num); AI=$($D epbydesc "$N boot interrogate" "Fixed 17")
$D mark "$N writes switch ep $SW number ep $NUM"
$D send "zcl send_gen write -d $short --dst-ep $SW -e 1 -c 0x0010 -a 0x0055 -t 0x10 -v 0x01" 4
$D send "zcl send_gen read -d $short --dst-ep $SW -e 1 -c 0x0010 -a 0x0055" 2
$D send "zcl send_gen write -d $short --dst-ep $NUM -e 1 -c 0x000d -a 0x0055 -t 0x39 -v 0x00002842" 4
$D send "zcl send_gen read -d $short --dst-ep $NUM -e 1 -c 0x000d -a 0x0055" 2
$D send "zcl send_gen write -d $short --dst-ep $SW -e 1 -c 0x0010 -a 0x0055 -t 0x10 -v 0x00" 4
$D mark "$N collecting reports for 75 s"
sleep 75
$D mark "$N final reads"
for e in $(eps_with 0xc); do $D send "zcl send_gen read -d $short --dst-ep $e -e 1 -c 0x000c -a 0x0055" 2; done
for e in $(eps_with 0xf); do $D send "zcl send_gen read -d $short --dst-ep $e -e 1 -c 0x000f -a 0x0055" 2; done
$D send "zcl send_gen read -d $short --dst-ep $SW -e 1 -c 0x0010 -a 0x0055" 2
$D send "zcl send_gen read -d $short --dst-ep $NUM -e 1 -c 0x000d -a 0x0055" 2
$D mark "$N done"
