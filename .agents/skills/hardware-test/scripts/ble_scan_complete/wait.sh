#!/bin/bash
# usage: wait.sh CAPTURE SECONDS [FROM_LINE]  block until an anomaly after FROM_LINE or SECONDS pass, then print status
C=$1; E=$(( $(date +%s) + $2 )); L=${3:-1}
P="Scan timeout exceeded|Scan never terminated|Dropped [0-9]+ BLE|Guru|Backtrace|Unexpected|cmpl_fail=[1-9]|bad=[1-9]"
while [ $(date +%s) -lt $E ]; do tail -n +$L $C | grep -a -q -E "$P" && break; sleep 15; done
tail -n +$L $C | grep -a -E "$P" | head -5
echo "$(date +%T) starts=$(tail -n +$L $C | grep -a -c 'Starting scan') completes=$(tail -n +$L $C | grep -a -c 'Scan complete, set')"
grep -a scaninst $C | tail -1 | cut -c1-260
