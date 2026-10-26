#!/bin/sh
# httpmix.sh HOST USER PASS ROUNDS : authenticated request mix (curl --anyauth follows the server's Basic or Digest
# challenge): page, two json endpoints, a switch POST, a 404, an unauthenticated 401 and a 2 s SSE stream per round
H=$1; U=$2; P=$3; N=$4; ok=0; bad=0
# chk EXPECTED LABEL curl-args... : only LABEL is printed on a miss, never the argument list with the credentials
chk() { exp=$1; label=$2; shift 2; c=$(curl -s -m 6 -o /dev/null -w "%{http_code}" "$@"); if [ "$c" = "$exp" ]; then ok=$((ok+1)); else bad=$((bad+1)); echo "expected $exp got $c: $label"; fi; }
sse_tmp=$(mktemp)
for i in $(seq 1 $N); do
  page=$(curl -s -m 6 -o /dev/null -w "%{http_code}" -u "$U:$P" --anyauth "http://$H/")
  if [ "$page" = 200 ]; then ok=$((ok+1)); else bad=$((bad+1)); echo "expected 200 got $page: /"; fi
  chk 200 "GET /sensor/Fast%201" -u "$U:$P" --anyauth "http://$H/sensor/Fast%201"
  chk 200 "GET /text_sensor/Big" -u "$U:$P" --anyauth "http://$H/text_sensor/Big"
  chk 200 "GET /switch/Toggle" -u "$U:$P" --anyauth "http://$H/switch/Toggle"
  chk 200 "POST /switch/Toggle/toggle" -u "$U:$P" --anyauth -X POST -H "Content-Length: 0" "http://$H/switch/Toggle/toggle"
  # The IDF server closes an unknown path without a response: curl ends with 52 (empty reply) or 56 (reset), never a
  # timeout (28) or a refused connection (7); only trust the close when the page just answered
  c=$(curl -s -m 6 -o /dev/null -w "%{http_code}" -u "$U:$P" --anyauth "http://$H/nonexistent"); rc=$?
  if [ "$c" = 000 ] && [ "$page" = 200 ] && { [ "$rc" = 52 ] || [ "$rc" = 56 ]; }; then ok=$((ok+1)); else bad=$((bad+1)); echo "unknown path: got $c, curl status $rc, with the page at $page"; fi
  chk 401 "GET / without credentials" "http://$H/"
  timeout 2 curl -s -N -u "$U:$P" --anyauth -o "$sse_tmp" "http://$H/events" 2>/dev/null; rc=$?
  if [ "$rc" = 124 ] && grep -q "^event: state" "$sse_tmp"; then ok=$((ok+1)); else bad=$((bad+1)); echo "SSE stream: curl status $rc, $(grep -c '^event: state' "$sse_tmp") state events"; fi
  sleep 1
done
rm -f "$sse_tmp"
echo "httpmix: $ok ok, $bad unexpected"
[ "$bad" = 0 ]
