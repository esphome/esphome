#!/bin/bash
# otaseq.sh DEVICE-IP FILE=EXPECT...: send each image with a config-less `esphome upload --file` while capturing the
# device log over USB. EXPECT is `reject` (a Signature check rejection and a failed upload) or the build tag the image
# must boot with (its `BUILDTAG <tag>` line and a successful upload). Exits 1 on any mismatch or missing verdict.
# Send the rejects first so every check runs in the same build.
# env: TREE (esphome checkout), PORT (device USB serial port), SC (path to serial_capture.py), PY (checkout python)
T=${TREE:?set TREE}; P=${PORT:?set PORT}; SC=${SC:?set SC}; PY=${PY:-$T/venv/bin/python}; IP=${1:?ip}; shift
for a in "$@"; do [ "${a%=*}" != "$a" ] || { echo "$a: give FILE=reject or FILE=<tag>"; exit 2; }; done
W=$(pwd); LOG=$W/ota-log.txt
pkill -f "[s]erial_capture.py $P"
: > "$LOG"
$PY "$SC" "$P" 1800 "$LOG" > /dev/null 2>&1 &
CAP=$!
fail=0
# first verdict line logged after line $1 of the capture
verdict() { tail -n +"$(($1 + 1))" "$LOG" | grep -a -o -E "Signature check: (image|no trusted)|BUILDTAG [A-Za-z0-9_.-]+" | head -1; }
for a in "$@"; do
  f=${a%=*}; want=${a##*=}
  kill -0 $CAP 2>/dev/null || { echo "serial capture died, stopping"; exit 1; }
  up=0; for i in $(seq 1 40); do nc -z -w2 "$IP" 3232 && { up=1; break; }; sleep 3; done
  [ $up = 1 ] || { echo "$f: device unreachable, stopping; not sent: $*"; fail=1; break; }
  start=$(wc -l < "$LOG")
  (cd "$T" && $PY -m esphome upload --file "$W/$f" --device "$IP" > "$W/up-$f.log" 2>&1)
  rc=$?
  shift
  v=""; for i in $(seq 1 60); do v=$(verdict "$start"); [ -n "$v" ] && break; sleep 1; done
  if [ "$want" = reject ]; then
    [[ "$v" == "Signature check"* && $rc != 0 ]] && res=ok || res=MISMATCH
  else
    [[ "$v" == "BUILDTAG $want" && $rc = 0 ]] && res=ok || res=MISMATCH
  fi
  [ $res = ok ] || fail=1
  echo "$f want=$want rc=$rc verdict=${v:-none} $res $(grep -a -E 'ERROR|OTA successful' "$W/up-$f.log" | tail -1 | cut -c1-100)"
done
kill $CAP; wait
grep -a -E "BUILDTAG|Signature check" "$LOG"
exit $fail
