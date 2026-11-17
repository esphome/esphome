#!/bin/bash
# run.sh NAME TREE YAML PORT BAUD ROUNDS: flash NAME, capture serial, drive the buttons and the soak
S=$(cd "$(dirname "$0")" && pwd); K=$(dirname "$(dirname "$S")"); N=$1; T=$2; Y=$3; P=$4; B=$5; R=$6
PY=${PY:-$T/venv/bin/python}
pkill -f "[s]erial_capture.py $P "
TREE=$T PIO_CORE=${PIO_CORE:-$S/pio} bash "$K/pio_build.sh" upload "$N" "$Y" "$P" || exit 1
rm -f $S/$N-serial.txt
$PY "$K/serial_capture.py" "$P" 260 $S/$N-serial.txt --baud $B --reset > /dev/null 2>&1 & CAP=$!
IP=""; for i in $(seq 60); do IP=$(grep -a -o 'IP Address: [0-9.]*' $S/$N-serial.txt 2>/dev/null | tail -1 | cut -d' ' -f3); [ -n "$IP" ] && break; sleep 2; done
[ -n "$IP" ] || IP=$(bash "$K/resolve.sh" "$N" 20) || { echo no ip; kill $CAP; exit 1; }
sleep 5
$PY $S/drive.py $IP $R soak > $S/$N-drive.txt 2>&1
sleep 3; kill $CAP; wait $CAP 2>/dev/null
cat $S/$N-drive.txt; grep -a -c "MARK tx" $S/$N-serial.txt
