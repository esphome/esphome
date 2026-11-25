#!/bin/bash
# runi.sh NAME YAML: fresh join, interrogate, bind and configure reporting, switch and number writes, 75 s of reports,
# final reads (c6-ents.yaml or nrf-ents.yaml). CHIP= (empty) for the nRF dongle. env: see common.sh
. "$(dirname "$0")/common.sh"; N=${1:?name}; Y=${2:?yaml}
join_fresh $N $Y 400
$D mark "$N bind and configure reporting"
for e in $($D eps "$N boot" 0xc); do bindrp $e 0x000c 0x0055 0x39 "--min 1 --max 30 --delta 0x00000000"; done
for e in $($D eps "$N boot" 0xf); do bindrp $e 0x000f 0x0055 0x10 "--min 1 --max 30"; done
read -r SW NUM < <($D epbydesc "$N boot interrogate" Sw Num); need "$SW" "$NUM"
$D mark "$N writes switch ep $SW number ep $NUM"
wr $SW 0x0010 0x10 0x01; rd $SW 0x0010
wf $NUM 42
wr $SW 0x0010 0x10 0x00
$D mark "$N collecting reports for 75 s"
sleep 75
$D mark "$N final reads"
for e in $($D eps "$N boot" 0xc); do rd $e 0x000c; done
for e in $($D eps "$N boot" 0xf); do rd $e 0x000f; done
rd $SW 0x0010; rd $NUM 0x000d
$D mark "$N done"
