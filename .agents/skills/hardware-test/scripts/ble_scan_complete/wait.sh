#!/bin/bash
# usage: wait.sh CAPTURE SECONDS [FROM_LINE]  block until an anomaly after FROM_LINE or SECONDS pass, then print status
C=$1; L=${3:-1}
P="Scan timeout exceeded|Scan never terminated|Dropped [0-9]+ BLE|Guru|Backtrace|Unexpected|cmpl_fail=[1-9]|bad=[1-9]"
timeout "$2" tail -n +"$L" -F "$C" 2> /dev/null | grep -a -m1 -q -E "$P"
tail -n +"$L" "$C" | awk -v p="$P" -v t="$(date +%T)" '
  $0 ~ p && shown < 5 { print; shown++ }
  /Starting scan/ { starts++ }
  /Scan complete, set/ { completes++ }
  END { printf "%s starts=%d completes=%d\n", t, starts, completes }'
grep -a scaninst "$C" | tail -1 | cut -c1-260
