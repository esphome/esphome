#!/bin/bash
# arena.sh NAME CONFIG KIND PORT: build CONFIG (bw15.yaml or bk7238.yaml) as NAME from TREE, flash it over serial PORT
# (KIND bw or bk), capture the serial log unless NOCAP is set, settle 60 s, run arena_drive.py, then print the resets,
# faults and lowest stack minimum from the log. Diagnostic: compare the per phase stack minimum between two builds.
# NOFLASH=1 skips the flash. NOCAP=1 skips the capture: under a VERY_VERBOSE flood a BW15 CH340 can drop off USB, and
# each reopen of its port resets the chip.
# env: TREE, PY, CREDS (default creds-short.yaml in ../configs), NOFLASH, NOCAP
A=$(cd "$(dirname "$0")" && pwd); W=$(dirname "$A"); S=$(dirname "$W")
N=${1:?name}; C=${2:?config}; K=${3:?kind}; P=${4:?port}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; CR=$W/configs/${CREDS:-creds-short.yaml}
read -r U PW < <($PY -c 'import sys, yaml; s = yaml.safe_load(open(sys.argv[1]))["substitutions"]; print(s["web_user"], s["web_pass"])' "$CR") ||
  { echo "cannot read $CR"; exit 1; }
"$S/pio_build.sh" compile "$N" "$A/$C" -s web_user "$U" -s web_pass "$PW" || exit 1
grep -q USE_JSON_ARENA "$A/.esphome/build/$N/src/esphome/core/defines.h" && echo "$N: USE_JSON_ARENA on" || echo "$N: USE_JSON_ARENA off"
IMG=$A/.esphome/build/$N/.pioenvs/$N/firmware.uf2
LTPY=$A/pio/penv/.libretiny/bin/python; LTL=$(ls -d "$A"/pio/platforms/libretiny* 2>/dev/null | head -1)
OPT=""
if [ -z "$NOFLASH" ]; then
  case $K in
    bw) OPT=--released
      timeout 600 "$LTPY" "$S/flash_rtl87xx_kit.py" -L "$LTL" flash write -d "$P" "$IMG" > "$A/$N-upload.log" 2>&1 ||
        { echo "$N flash failed (see $A/$N-upload.log)"; exit 1; } ;;
    bk) FRC=1; for _ in 1 2 3; do timeout 400 "$LTPY" "$S/flash_bk72xx.py" -L "$LTL" flash write -d "$P" "$IMG" > "$A/$N-upload.log" 2>&1 && { FRC=0; break; }; done
      [ $FRC = 0 ] || { echo "$N flash failed (see $A/$N-upload.log)"; exit 1; } ;;
    *) echo "unknown kind $K"; exit 1 ;;
  esac
else
  [ "$K" = bw ] && OPT=--released
fi
pkill -f "[s]erial_capture.py $P "; sleep 1
L=$A/$N-serial.log; : > "$L"
if [ -z "$NOCAP" ]; then $PY "$S/serial_capture.py" "$P" 2400 "$L" $OPT 2> "$A/$N-cap.err" & CAP=$!; trap 'kill $CAP 2>/dev/null' EXIT; fi
H=$("$S/resolve.sh" "$N" 60) || exit 1
echo "$H" > "$A/$N-host.txt"
echo "== $N settle 60 s"; sleep 60
$PY -I "$W/arena_drive.py" "$H" "$U" "$PW" 60
sleep 3; [ -n "$CAP" ] && { kill $CAP 2>/dev/null; wait $CAP 2>/dev/null; }
echo "== $N serial: resets $(grep -a -c -E 'Reset Reason|ROM Version' "$L"), port losses $(grep -a -c 'port lost' "$L")," \
  "faults $(grep -a -c -i -E 'fault|crash|assert|abort|stack overflow' "$L")"
grep -a "\[hwm" "$L" | sed -n '1p;$p'
grep -a "\[hwm" "$L" | sed 's/.*stack min \([0-9-]*\) B.*/\1/' | sort -n | head -1 | sed 's/^/serial lowest stack min: /'
