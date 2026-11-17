#!/bin/bash
# run.sh NAME KIND PORT: serial flash NAME (built from TREE into this directory), capture, flow.py, web OTA with its
# own image plus a restart, then crash and boot counts. KIND is esp, pico, bk or bw. env: TREE, PY
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); N=${1:?name}; K=${2:?kind}; P=${3:?port}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
B=$D/.esphome/build/$N/.pioenvs/$N
pkill -f "[s]erial_capture.py $P "; sleep 1
LTPY=$D/pio/penv/.libretiny/bin/python; [ -x "$LTPY" ] || LTPY=~/.platformio/penv/.libretiny/bin/python
LTL=$(ls -d "$D"/pio/platforms/libretiny* 2> /dev/null | head -1)
LOPT=(); [ -n "$LTL" ] && LOPT=(-L "$LTL")
case $K in
  esp | pico)
    PIO_CORE=${PIO_CORE:-$D/pio} bash "$S/pio_build.sh" upload "$N" "$D/$N.yaml" "$P" || exit 1
    IMG=$B/firmware.bin; OPT=() ;;
  bk)
    for _ in 1 2 3; do
      timeout 400 "$LTPY" "$S/flash_bk72xx.py" "${LOPT[@]}" flash write -d "$P" "$B/firmware.uf2" > "$D/$N-upload.log" 2>&1 &&
        break
    done
    tail -2 "$D/$N-upload.log"; IMG=$B/firmware.uf2; OPT=() ;;
  bw)
    timeout 600 "$LTPY" "$S/flash_rtl87xx_kit.py" "${LOPT[@]}" flash write -d "$P" "$B/firmware.uf2" > "$D/$N-upload.log" 2>&1
    echo "flash rc=$?"; tail -2 "$D/$N-upload.log"; IMG=$B/firmware.uf2; OPT=(--released) ;;
  *) echo "unknown kind $K"; exit 1 ;;
esac
[ "$K" = pico ] && sleep 6
: > "$D/$N-serial.log"; ($PY "$S/serial_capture.py" "$P" 1800 "$D/$N-serial.log" "${OPT[@]}" 2> "$D/$N-cap.err" &)
HOST=$(bash "$S/resolve.sh" "$N" 60) || { echo "no host"; exit 1; }
echo "$HOST" > "$D/$N-host.txt"; sleep 10
$PY -I "$D/flow.py" "$HOST" "$D/$N"
bash "$D/webota.sh" "$HOST" "$IMG" "$D/$N"
sleep 5; pkill -f "[s]erial_capture.py $P "; sleep 1
L=$D/$N-serial.log
echo "SERIAL crash lines $(grep -a -c -E 'Panic |Exception \(|Fatal exception|HardFault|Hard Fault|CRASH|abort\(\)|Soft WDT|wdt reset|assert|Backtrace' "$L")," \
  "boots $(grep -a -c -E 'Log initialized' "$L"), port losses $(grep -a -c 'port lost' "$L"), lines $(wc -l < "$L")"
