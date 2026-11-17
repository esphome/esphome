#!/bin/bash
# run.sh NAME KIND PORT: serial flash NAME (built from TREE into this directory), capture, flow.py, web OTA with its
# own image plus a restart, then crash and boot counts. KIND is esp, pico, bk or bw. env: TREE, PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); N=${1:?name}; K=${2:?kind}; P=${3:?port}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
B=$D/.esphome/build/$N/.pioenvs/$N
pkill -f "[s]erial_capture.py $P "; sleep 1
. "$D/ltflash.sh"
case $K in
  esp | pico)
    PIO_CORE=${PIO_CORE:-$D/pio} bash "$S/pio_build.sh" upload "$N" "$D/$N.yaml" "$P" || exit 1
    IMG=$B/firmware.bin; OPT=() ;;
  bk | bw)
    ltflash "$K" "$P" "$B/firmware.uf2" "$D/$N-upload.log" || { echo "flash failed"; tail -2 "$D/$N-upload.log"; exit 1; }
    tail -2 "$D/$N-upload.log"; IMG=$B/firmware.uf2; OPT=()
    [ "$K" = bw ] && OPT=(--released) ;;
  *) echo "unknown kind $K"; exit 1 ;;
esac
[ "$K" = pico ] && sleep 6
: > "$D/$N-serial.log"; ($PY "$S/serial_capture.py" "$P" 1800 "$D/$N-serial.log" "${OPT[@]}" 2> "$D/$N-cap.err" &)
HOST=$(bash "$S/resolve.sh" "$N" 60) || { echo "no host"; exit 1; }
echo "$HOST" > "$D/$N-host.txt"; sleep 10
$PY -I "$D/flow.py" "$HOST" "$D/$N"
PY=$PY bash "$D/webota.sh" "$HOST" "$IMG" "$D/$N"
sleep 5; pkill -f "[s]erial_capture.py $P "; sleep 1
L=$D/$N-serial.log
echo "SERIAL crash lines $(grep -a -c -E 'Panic |Exception \(|Fatal exception|HardFault|Hard Fault|CRASH|abort\(\)|Soft WDT|wdt reset|assert|Backtrace' "$L")," \
  "boots $(grep -a -c -E 'Log initialized' "$L"), port losses $(grep -a -c 'port lost' "$L"), lines $(wc -l < "$L")"
