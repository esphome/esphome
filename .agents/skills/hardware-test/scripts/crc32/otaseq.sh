#!/bin/bash
# otaseq.sh TREE CONFIG DEVICE-IP LOG-PORT FILE...: send each image with esphome upload --file and capture the
# device log over USB the whole time; expect the rejects first so every check runs in the same firmware.
set -u
TREE=$1; CFG=$2; IP=$3; PORT=$4; shift 4
SKILL=$(cd "$(dirname "$0")/../.." && pwd); PY=${PY:-python3}; W=$(pwd)
$PY "$SKILL/scripts/serial_capture.py" "$PORT" 900 ota-log.txt > /dev/null 2>&1 &
CAP=$!
for f in "$@"; do
  for i in $(seq 1 40); do nc -z -w2 "$IP" 3232 && break; sleep 3; done
  (cd "$TREE" && PYTHONPATH=$TREE $PY -m esphome upload "$CFG" --device "$IP" --file "$W/$f" > "$W/up-$f.log" 2>&1)
  echo "$f rc=$? $(grep -a -E 'ERROR|OTA successful' "up-$f.log" | tail -1 | cut -c1-120)"
  sleep 25
done
kill $CAP; wait
grep -a -E "BUILDTAG|Signature check" ota-log.txt
