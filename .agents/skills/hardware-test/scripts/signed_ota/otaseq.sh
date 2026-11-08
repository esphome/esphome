#!/bin/bash
# otaseq.sh DEVICE-IP FILE...: send each image with a config-less `esphome upload --file` while capturing the
# device log over USB; after each, wait for its verdict line (Signature check rejection or the new BUILDTAG).
# Send the rejects first so every check runs in the same build.
# env: TREE (esphome checkout), PORT (device USB serial port), SC (path to serial_capture.py), PY (checkout python)
T=${TREE:?set TREE}; P=${PORT:?set PORT}; SC=${SC:?set SC}; PY=${PY:-$T/venv/bin/python}; IP=${1:?ip}; shift
W=$(pwd); LOG=$W/ota-log.txt
pkill -f "[s]erial_capture.py $P"
$PY "$SC" "$P" 1800 "$LOG" > /dev/null 2>&1 &
CAP=$!
verdicts() { grep -a -c -E "Signature check: (image|no trusted)|BUILDTAG" "$LOG" 2>/dev/null; }
for f in "$@"; do
  for i in $(seq 1 40); do nc -z -w2 "$IP" 3232 && break; sleep 3; done
  before=$(verdicts)
  (cd "$T" && $PY -m esphome upload --file "$W/$f" --device "$IP" > "$W/up-$f.log" 2>&1)
  echo "$f rc=$? $(grep -a -E 'ERROR|OTA successful' "$W/up-$f.log" | tail -1 | cut -c1-120)"
  for i in $(seq 1 60); do [ "$(verdicts)" -gt "$before" ] && break; sleep 1; done
done
kill $CAP; wait
grep -a -E "BUILDTAG|Signature check" "$LOG"
