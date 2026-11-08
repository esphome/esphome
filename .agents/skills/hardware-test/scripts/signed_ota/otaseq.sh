#!/bin/bash
# otaseq.sh DEVICE-IP FILE...: send each image with a config-less `esphome upload --file` while capturing the
# device log over USB; after each, wait for its verdict line (Signature check rejection or the new BUILDTAG).
# Send the rejects first so every check runs in the same build.
# env: TREE (esphome checkout), PORT (device USB serial port), SC (path to serial_capture.py), PY (checkout python)
T=${TREE:?set TREE}; P=${PORT:?set PORT}; SC=${SC:?set SC}; PY=${PY:-$T/venv/bin/python}; IP=${1:?ip}; shift
W=$(pwd); LOG=$W/ota-log.txt
pkill -f "[s]erial_capture.py $P"
: > "$LOG"
$PY "$SC" "$P" 1800 "$LOG" > /dev/null 2>&1 &
CAP=$!
fail=0
verdicts() { grep -a -c -E "Signature check: (image|no trusted)|BUILDTAG" "$LOG" || true; }
for f in "$@"; do
  kill -0 $CAP 2>/dev/null || { echo "serial capture died, stopping"; exit 1; }
  up=0; for i in $(seq 1 40); do nc -z -w2 "$IP" 3232 && { up=1; break; }; sleep 3; done
  [ $up = 1 ] || { echo "$f: device unreachable, stopping; not sent: $*"; fail=1; break; }
  before=$(verdicts)
  (cd "$T" && $PY -m esphome upload --file "$W/$f" --device "$IP" > "$W/up-$f.log" 2>&1)
  rc=$?
  shift
  got=0; for i in $(seq 1 60); do [ "$(verdicts)" -gt "$before" ] && { got=1; break; }; sleep 1; done
  [ $got = 1 ] || fail=1
  echo "$f rc=$rc $([ $got = 1 ] || echo 'NO VERDICT ')$(grep -a -E 'ERROR|OTA successful' "$W/up-$f.log" | tail -1 | cut -c1-120)"
done
kill $CAP; wait
grep -a -E "BUILDTAG|Signature check" "$LOG"
exit $fail
