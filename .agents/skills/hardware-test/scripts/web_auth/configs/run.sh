#!/bin/bash
# run.sh NAME CONFIG KIND PORT MODE: build CONFIG (a file in this directory) as NAME from TREE, flash it over serial
# (unless MODE contains noflash), capture the serial log, run authfuzz.py and count crash lines.
#   KIND esp|pico|bk|bw; MODE full (cases, heavy, timing and an OTA of its own image) | cases
#   Exit 0 only when every authfuzz case passed and the serial log shows no crash; 1 otherwise.
#   env: TREE, PY, CREDS (default creds-short.yaml; its web_user and web_pass are built in and used by authfuzz)
D=$(cd "$(dirname "$0")" && pwd); N=${1:?name}; C=${2:?config}; K=${3:?kind}; P=${4:?port}; M=${5:?mode}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; S=$(dirname "$(dirname "$D")")
CR=$D/${CREDS:-creds-short.yaml}
read -r U W < <($PY -c 'import sys, yaml; s = yaml.safe_load(open(sys.argv[1]))["substitutions"]; print(s["web_user"], s["web_pass"])' "$CR") ||
  { echo "cannot read $CR"; exit 1; }
"$S/pio_build.sh" compile "$N" "$D/$C" -s web_user "$U" -s web_pass "$W" || exit 1
B=$D/.esphome/build/$N/.pioenvs/$N
LTPY=$D/pio/penv/.libretiny/bin/python; LTL=$(ls -d "$D"/pio/platforms/libretiny* 2>/dev/null | head -1)
OPT=""
case $K in
  esp|pico) IMG=$B/firmware.bin ;;
  bk) IMG=$B/firmware.uf2 ;;
  bw) IMG=$B/firmware.uf2; OPT="--released" ;;
  *) echo "unknown kind $K"; exit 1 ;;
esac
pkill -f "[s]erial_capture.py $P "; sleep 1
if [[ $M != *noflash* ]]; then
  case $K in
    esp|pico) "$S/pio_build.sh" upload "$N" "$D/$C" "$P" -s web_user "$U" -s web_pass "$W" || exit 1 ;;
    bk) FRC=1; for _ in 1 2 3; do timeout 400 "$LTPY" "$S/flash_bk72xx.py" -L "$LTL" flash write -d "$P" "$IMG" > "$D/$N-upload.log" 2>&1 && { FRC=0; break; }; done
      [ $FRC = 0 ] || { echo "$N flash failed (see $D/$N-upload.log)"; exit 1; } ;;
    bw) timeout 600 "$LTPY" "$S/flash_rtl87xx_kit.py" -L "$LTL" flash write -d "$P" "$IMG" > "$D/$N-upload.log" 2>&1 ||
      { echo "$N flash failed (see $D/$N-upload.log)"; exit 1; } ;;
  esac
fi
[ "$K" = pico ] && { sleep 3; for _ in $(seq 1 30); do [ -e "$P" ] && break; sleep 1; done; sleep 2; }
L=$D/$N-serial.log
$PY "$S/serial_capture.py" "$P" 3000 "$L" $OPT 2> "$D/$N-cap.err" &
CAP=$!
trap 'kill $CAP 2>/dev/null' EXIT
HOST=$("$S/resolve.sh" "$N" 60) || exit 1
echo "$HOST" > "$D/$N-host.txt"; sleep 8
case $M in
  full*) $PY -I "$S/web_auth/authfuzz.py" "$HOST" "$U" "$W" "$D/$N" --ota "$IMG" ;;
  *) $PY -I "$S/web_auth/authfuzz.py" "$HOST" "$U" "$W" "$D/$N" --only cases ;;
esac
AF=$?
sleep 5
# a capture that died early watched nothing, so its empty log must not count as a clean run
CAPOK=1; kill -0 $CAP 2>/dev/null || { CAPOK=0; echo "the capture ended before the run did (see $D/$N-cap.err)"; }
kill $CAP 2>/dev/null; wait $CAP 2>/dev/null; trap - EXIT
CRASH=$(grep -a -c -E 'Panic |Exception \(|Fatal exception|HardFault|Hard Fault|CRASH|abort\(\)|Soft WDT|wdt reset|assert|Backtrace' "$L")
echo "SERIAL crash lines $CRASH," \
  "boots $(grep -a -c -E 'Log initialized|Reset reason' "$L"), port losses $(grep -a -c 'port lost' "$L"), lines $(wc -l < "$L")" | tee "$D/$N-serialsum.txt"
grep -a "Heap free" "$L" | sed -n '1p;$p' | tee -a "$D/$N-serialsum.txt"
[ "$AF" = 0 ] && [ "$CRASH" = 0 ] && [ $CAPOK = 1 ] || { echo "FAIL (authfuzz exit $AF, crash lines $CRASH, capture ok $CAPOK)"; exit 1; }
echo PASS
