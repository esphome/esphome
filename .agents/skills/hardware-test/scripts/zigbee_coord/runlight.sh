#!/bin/bash
# runlight.sh NAME YAML: fresh join, bind and report On/Off, Level and Color, drive the three lights with raw
# commands, then wait for the local changes from lights-local.yaml (c6-lights.yaml). env: TREE, PY, PORT, COORD_IEEE
Z=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; Y=${2:?yaml}; D="${PY:-python3} $Z/zbdrv.py"
: "${COORD_IEEE:?set COORD_IEEE (address -x on the coordinator)}"
since=$($D lines); $D mark "$N start"
CHIP=${CHIP-esp32c6} $Z/flashdev.sh $N $Y 420 || exit 1
t0=$(date +%s)
short=$($D waitjoin $since 120); echo "joined $short"; [ "$short" = NOJOIN ] && exit 2
sleep 6
$D interrogate "$N boot" $short | tee $Z/int-$N.txt
LED=$(grep "0x300" $Z/int-$N.txt | awk '{print $2}' | head -1)
MONO=$(grep "'0x8'" $Z/int-$N.txt | grep -v "0x300" | awk '{print $2}' | head -1)
BIN=$(grep "'0x6'" $Z/int-$N.txt | grep -v "'0x8'" | awk '{print $2}' | head -1)
echo "led $LED mono $MONO bin $BIN"
$D mark "$N bind led $LED mono $MONO bin $BIN"
for e in $LED $MONO $BIN; do
  $D send "zdo bind -c 0x0006 -S $e -D 2 -s $short -d $COORD_IEEE" 3
  $D send "zcl send_gen config_rp -d $short --dst-ep $e -e 2 -c 0x0006 -a 0x0000 -t 0x10 --min 0 --max 300" 3
done
for e in $LED $MONO; do
  $D send "zdo bind -c 0x0008 -S $e -D 2 -s $short -d $COORD_IEEE" 3
  $D send "zcl send_gen config_rp -d $short --dst-ep $e -e 2 -c 0x0008 -a 0x0000 -t 0x20 --min 0 --max 300 --delta 0x01" 3
done
$D send "zdo bind -c 0x0300 -S $LED -D 2 -s $short -d $COORD_IEEE" 3
$D send "zcl send_gen config_rp -d $short --dst-ep $LED -e 2 -c 0x0300 -a 0x0003 -t 0x21 --min 0 --max 300 --delta 0x0001" 3
$D send "zcl send_gen config_rp -d $short --dst-ep $LED -e 2 -c 0x0300 -a 0x0004 -t 0x21 --min 0 --max 300 --delta 0x0001" 3
rd() { $D send "zcl send_gen read -d $short --dst-ep $1 -e 2 -c $2 -a $3" 1.8; }
$D mark "$N cmd bin on"
$D send "zcl send_raw -d $short --dst-ep $BIN -e 2 -c 0x0006 --cmd 1" 3; rd $BIN 0x0006 0x0000
$D mark "$N cmd mono level 128"
$D send "zcl send_raw -d $short --dst-ep $MONO -e 2 -c 0x0008 --cmd 4 -p 0x800000" 4; rd $MONO 0x0006 0x0000; rd $MONO 0x0008 0x0000
$D mark "$N cmd led on, level 200, color red"
$D send "zcl send_raw -d $short --dst-ep $LED -e 2 -c 0x0006 --cmd 1" 3
$D send "zcl send_raw -d $short --dst-ep $LED -e 2 -c 0x0008 --cmd 0 -p 0xc80000" 3
$D send "zcl send_raw -d $short --dst-ep $LED -e 2 -c 0x0300 --cmd 7 -p 0x33b38b4c0000" 4
rd $LED 0x0006 0x0000; rd $LED 0x0008 0x0000; rd $LED 0x0300 0x0003; rd $LED 0x0300 0x0004; rd $LED 0x0300 0x0008
$D mark "$N cmd led color green"
$D send "zcl send_raw -d $short --dst-ep $LED -e 2 -c 0x0300 --cmd 7 -p 0x8f4ccc990000" 4   # x=0.30 y=0.60
rd $LED 0x0300 0x0003; rd $LED 0x0300 0x0004
$D mark "$N cmd bin toggle, mono off"
$D send "zcl send_raw -d $short --dst-ep $BIN -e 2 -c 0x0006 --cmd 2" 3; rd $BIN 0x0006 0x0000
$D send "zcl send_raw -d $short --dst-ep $MONO -e 2 -c 0x0006 --cmd 0" 3; rd $MONO 0x0006 0x0000; rd $MONO 0x0008 0x0000
el=$(( $(date +%s) - t0 )); w=$(( 200 - el )); [ $w -gt 0 ] && { $D mark "$N waiting ${w}s for local changes"; sleep $w; }
$D mark "$N final reads"
for e in $LED $MONO $BIN; do rd $e 0x0006 0x0000; done
rd $LED 0x0008 0x0000; rd $MONO 0x0008 0x0000; rd $LED 0x0300 0x0003; rd $LED 0x0300 0x0004
$D mark "$N done"
