#!/bin/bash
# stress.sh HOST SECONDS: 8 parallel pings with 1400 byte payloads every 0.1 s (about 110 KB/s each way) for SECONDS.
# Exits 1 when a ping fails before the end or none of its pings was answered (errors in stress.err); `timeout`
# bounds it on macOS and Linux alike, with SIGINT so ping prints its received count.
: > stress.err
pids=()
for i in 1 2 3 4 5 6 7 8; do
  timeout -s INT "$2" ping -q -s 1400 -i 0.1 "$1" > "stress.$i.out" 2>> stress.err &
  pids+=($!)
done
rc=0
for i in 1 2 3 4 5 6 7 8; do
  wait "${pids[$((i - 1))]}"; s=$?
  [ $s = 124 ] || { echo "a ping exited early with $s, see stress.err" >&2; rc=1; }
  grep -Eq ' [1-9][0-9]* (packets )?received' "stress.$i.out" \
    || { echo "a ping got no replies: no Wi-Fi load" >&2; rc=1; }
  rm -f "stress.$i.out"
done
exit $rc
