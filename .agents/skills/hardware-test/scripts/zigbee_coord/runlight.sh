#!/bin/bash
# runlight.sh NAME YAML: fresh join, bind and report On/Off, Level and Color, drive the three lights with raw
# commands, then wait for the local changes from lights-local.yaml (c6-lights.yaml). env: see common.sh
. "$(dirname "$0")/common.sh"; N=${1:?name}; Y=${2:?yaml}; SRC=2
join_fresh $N $Y 420
LED=$($D eps "$N boot" 0x300 | head -1); MONO=$($D eps "$N boot" 0x8 0x300 | head -1)
BIN=$($D eps "$N boot" 0x6 0x8 | head -1)
echo "led $LED mono $MONO bin $BIN"; need "$LED" "$MONO" "$BIN"
$D mark "$N bind led $LED mono $MONO bin $BIN"
for e in $LED $MONO $BIN; do bindrp $e 0x0006 0x0000 0x10 "--min 0 --max 300"; done
for e in $LED $MONO; do bindrp $e 0x0008 0x0000 0x20 "--min 0 --max 300 --delta 0x01"; done
bindrp $LED 0x0300 0x0003 0x21 "--min 0 --max 300 --delta 0x0001"
$D send "zcl send_gen config_rp -d $short --dst-ep $LED -e 2 -c 0x0300 -a 0x0004 -t 0x21 --min 0 --max 300 \
  --delta 0x0001" 3
# raw EP CL CMD [PAYLOAD] [WAIT]
raw() { $D send "zcl send_raw -d $short --dst-ep $1 -e 2 -c $2 --cmd $3 ${4:+-p $4}" "${5:-3}"; }
$D mark "$N cmd bin on"
raw $BIN 0x0006 1; rd $BIN 0x0006 0x0000
$D mark "$N cmd mono level 128"
raw $MONO 0x0008 4 0x800000 4; rd $MONO 0x0006 0x0000; rd $MONO 0x0008 0x0000
$D mark "$N cmd led on, level 200, color red"
raw $LED 0x0006 1; raw $LED 0x0008 0 0xc80000; raw $LED 0x0300 7 0x33b38b4c0000 4
rd $LED 0x0006 0x0000; rd $LED 0x0008 0x0000; rd $LED 0x0300 0x0003; rd $LED 0x0300 0x0004; rd $LED 0x0300 0x0008
$D mark "$N cmd led color green"
raw $LED 0x0300 7 0x8f4ccc990000 4  # x=0.30 y=0.60
rd $LED 0x0300 0x0003; rd $LED 0x0300 0x0004
$D mark "$N cmd bin toggle, mono off"
raw $BIN 0x0006 2; rd $BIN 0x0006 0x0000
raw $MONO 0x0006 0; rd $MONO 0x0006 0x0000; rd $MONO 0x0008 0x0000
# lights-local.yaml makes its changes 150 s and 170 s after boot
w=$(( 185 - ($(date +%s) - booted) )); [ $w -gt 0 ] && { $D mark "$N waiting ${w}s for local changes"; sleep $w; }
$D mark "$N final reads"
for e in $LED $MONO $BIN; do rd $e 0x0006 0x0000; done
rd $LED 0x0008 0x0000; rd $MONO 0x0008 0x0000; rd $LED 0x0300 0x0003; rd $LED 0x0300 0x0004
$D mark "$N done"
