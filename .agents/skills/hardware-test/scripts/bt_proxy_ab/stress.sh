#!/bin/bash
# stress.sh HOST SECONDS: 8 parallel pings with 1400 byte payloads every 0.1 s (about 110 KB/s each way) for SECONDS.
# Exits 1 when a ping fails before the end (errors in stress.err); `timeout` bounds it on macOS and Linux alike.
: > stress.err
pids=()
for _ in 1 2 3 4 5 6 7 8; do
  timeout "$2" ping -q -s 1400 -i 0.1 "$1" > /dev/null 2>> stress.err &
  pids+=($!)
done
rc=0
for p in "${pids[@]}"; do
  wait "$p"; s=$?
  [ $s = 124 ] || { echo "a ping exited early with $s, see stress.err" >&2; rc=1; }
done
exit $rc
