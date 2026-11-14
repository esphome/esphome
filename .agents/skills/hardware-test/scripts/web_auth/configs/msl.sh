#!/bin/bash
# msl.sh NAME CONFIG PORT [noflash]: lwIP TIME_WAIT check (#22696, TCP_MSL) on a BW15. Builds CONFIG (bw15.yaml) as NAME
# from TREE, flashes it over serial PORT, captures the serial log, settles 90 s, then runs heap_churn.py (600 wrong
# credential requests at 3 per second, heap every 10 s and every 1 s for 20 s after the load, 150 s after) and the
# 32 KB header and 64 KB line cases of isolate.py, and counts the resets in the log. Exits 1 when either script fails,
# the capture ended early or the log shows a fault or more than the one reset that opening the port can cause. Compare
# the recovery time and the lowest heap between two builds. env: TREE, PY, CREDS (default creds-short.yaml)
D=$(cd "$(dirname "$0")" && pwd); W=$(dirname "$D"); S=$(dirname "$W")
N=${1:?name}; C=${2:?config}; P=${3:?port}
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; CR=$D/${CREDS:-creds-short.yaml}
read -r U PW < <($PY -c 'import sys, yaml; s = yaml.safe_load(open(sys.argv[1]))["substitutions"]; print(s["web_user"], s["web_pass"])' "$CR") ||
  { echo "cannot read $CR"; exit 1; }
"$S/pio_build.sh" compile "$N" "$D/$C" -s web_user "$U" -s web_pass "$PW" || exit 1
if [ "$4" != noflash ]; then
  PC=${PIO_CORE:-$D/pio}; LTPY=$PC/penv/.libretiny/bin/python; LTL=$(ls -d "$PC"/platforms/libretiny* 2>/dev/null | head -1)
  timeout 600 "$LTPY" "$S/flash_rtl87xx_kit.py" -L "$LTL" flash write -d "$P" "$D/.esphome/build/$N/.pioenvs/$N/firmware.uf2" \
    > "$D/$N-upload.log" 2>&1 || { echo "$N flash failed (see $D/$N-upload.log)"; exit 1; }
fi
pkill -f "[s]erial_capture.py $P "; sleep 1
L=$D/$N-serial.log; : > "$L"
$PY "$S/serial_capture.py" "$P" 3000 "$L" --released 2> "$D/$N-cap.err" &
CAP=$!; trap 'kill $CAP 2>/dev/null' EXIT
H=$("$S/resolve.sh" "$N" 60) || exit 1
echo "$H" > "$D/$N-host.txt"
echo "== $N settle 90 s after boot"; sleep 90
RC=0
echo "== $N heap timeline, 600 wrong-credential requests at 3/s"
$PY -I "$W/heap_churn.py" "$H" "$U" "$PW" 600 3 10 150 || { echo "FAIL: heap_churn.py"; RC=1; }
echo "== $N 32 KB header and 64 KB line"
$PY -I "$W/isolate.py" "$H" "$U" "$PW" 1 garbage_32k,nonl_64k,control || { echo "FAIL: isolate.py"; RC=1; }
sleep 3
kill -0 $CAP 2>/dev/null || { echo "FAIL: the serial capture ended early (see $D/$N-cap.err)"; RC=1; }
kill $CAP 2>/dev/null; wait $CAP 2>/dev/null; trap - EXIT
# a live capture that read nothing would look clean
[ "$(grep -a -c -v '^\[[0-9:.]*\]\[capture\]' "$L")" -gt 0 ] || { echo "FAIL: no firmware lines in $L"; RC=1; }
RS=$(grep -a -c -E 'Reset Reason|ROM Version' "$L")
# fault only as a word start, so the light's "Default Transition Length" line does not count
FL=$(grep -a -c -i -E '(^|[^[:alpha:]])fault|hardfault|crash|assert|abort|stack overflow' "$L")
echo "== $N serial resets: $RS, port losses: $(grep -a -c 'port lost' "$L"), faults: $FL"
# Opening the capture can reset the chip once, so one boot is expected
{ [ "$FL" -gt 0 ] || [ "$RS" -gt 1 ]; } && { echo "FAIL: faults $FL, resets $RS"; RC=1; }
exit $RC
