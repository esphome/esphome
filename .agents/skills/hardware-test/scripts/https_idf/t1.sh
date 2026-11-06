#!/bin/bash
# t1.sh HOST: requests against the HTTPS listener on 443: one GET, 20 sequential, 6 parallel three times, a 16 KB
# body and /events (answered 501 on HTTPS). PASS when every /hw request got a 200 and /events a 501, FAIL otherwise.
H=${1:?device address}; c() { curl -sk --max-time 10 "$@"; }
FAIL=0
echo "== https GET"; c -w " [%{http_code} tls %{time_appconnect}s total %{time_total}s]\n" "https://$H/hw"
echo "== 20 sequential"
S=$(for _ in $(seq 20); do c -o /dev/null -w "%{http_code} %{time_total}\n" "https://$H/hw"; done)
echo "$S" | awk '{n[$1]++; s+=$2; if($2>m)m=$2} END{for(k in n) printf "%s x%d ", k, n[k]; printf "avg %.2fs max %.2fs\n", s/NR, m}'
echo "$S" | grep -q -v '^200 ' && FAIL=1
echo "== 6 parallel x3"
S=$(for _ in 1 2 3; do for _ in $(seq 6); do c -o /dev/null -w "%{http_code}\n" "https://$H/hw" & done; wait; done)
echo "$S" | sort | uniq -c; echo "$S" | grep -q -v '^200$' && FAIL=1
echo "== big 16 KB"; B=$(c -o /dev/null -w "%{http_code} %{size_download}" "https://$H/hw/big"); echo "$B B"
[ "$B" = "200 16383" ] || FAIL=1
E=$(c -o /dev/null -w "%{http_code}" "https://$H/events"); echo "== /events: $E"; [ "$E" = 501 ] || FAIL=1
c "https://$H/hw"
[ "$FAIL" = 0 ] && echo PASS || { echo "FAIL: a request did not get the expected answer"; exit 1; }
