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
    # both kits wire RTS to reset: a capture that asserts it holds the board in reset
    tail -2 "$D/$N-upload.log"; IMG=$B/firmware.uf2; OPT=(--released) ;;
  *) echo "unknown kind $K"; exit 1 ;;
esac
[ "$K" = pico ] && sleep 6
: > "$D/$N-serial.log"; ($PY "$S/serial_capture.py" "$P" 1800 "$D/$N-serial.log" "${OPT[@]}" 2> "$D/$N-cap.err" &)
HOST=$(bash "$S/resolve.sh" "$N" 60) || { echo "no host"; exit 1; }
echo "$HOST" > "$D/$N-host.txt"; sleep 10
$PY -I "$D/flow.py" "$HOST" "$D/$N"; RC=$?
PY=$PY bash "$D/webota.sh" "$HOST" "$IMG" "$D/$N" || RC=1
sleep 5; pkill -f "[s]erial_capture.py $P "; sleep 1
L=$D/$N-serial.log
CR=$(grep -a -c -E 'Panic |Exception \(|Fatal exception|HardFault|Hard Fault|CRASH|abort\(\)|Soft WDT|wdt reset|assert|Backtrace' "$L")
[ "$CR" = 0 ] || RC=1
BOOTS=$(grep -a -c -E 'Log initialized|ESPHome version .* compiled on' "$L"); LOST=$(grep -a -c 'port lost' "$L")
echo "SERIAL crash lines $CR, boots $BOOTS, port losses $LOST, lines $(wc -l < "$L")"
# the web OTA and the restart press each reboot the board, so a capture with no boot saw nothing; a native USB
# port (Pico) can lose the boot banner, but then the capture shows the port going away
[ -s "$L" ] && [ $((BOOTS + LOST)) -gt 0 ] || { echo "SERIAL capture empty or shows no boot: $(tail -1 "$D/$N-cap.err")"; RC=1; }
exit $RC
