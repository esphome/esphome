#!/bin/sh
# httpmix.sh HOST USER PASS ROUNDS : authenticated request mix (curl --anyauth follows the server's Basic or Digest
# challenge): page, two json endpoints, a switch POST, a 404, an unauthenticated 401 and a 2 s SSE stream per round
H=$1; U=$2; P=$3; N=$4; ok=0; bad=0
chk() { exp=$1; shift; c=$(curl -s -m 6 -o /dev/null -w "%{http_code}" "$@"); if [ "$c" = "$exp" ]; then ok=$((ok+1)); else bad=$((bad+1)); echo "expected $exp got $c: $*" | sed 's/-u [^ ]* //'; fi; }
for i in $(seq 1 $N); do
  chk 200 -u "$U:$P" --anyauth "http://$H/"
  chk 200 -u "$U:$P" --anyauth "http://$H/sensor/Fast%201"
  chk 200 -u "$U:$P" --anyauth "http://$H/text_sensor/Big"
  chk 200 -u "$U:$P" --anyauth "http://$H/switch/Toggle"
  chk 200 -u "$U:$P" --anyauth -X POST -H "Content-Length: 0" "http://$H/switch/Toggle/toggle"
  chk 000 -u "$U:$P" --anyauth "http://$H/nonexistent"
  chk 401 "http://$H/"
  timeout 2 curl -s -N -u "$U:$P" --anyauth "http://$H/events" > /dev/null 2>&1
  sleep 1
done
echo "httpmix: $ok ok, $bad unexpected"
