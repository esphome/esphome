#!/bin/bash
# t2.sh HOST: switch the listener at run time: to http:80 and back, to https:8443 with a stalled TLS client and an
# idle socket open, 60 requests for a heap check, then a key that does not match the certificate (the server must
# log the error and stay off). Exits 1 when a listener that should answer did not.
H=${1:?device address}; c() { curl -sk --max-time 10 "$@"; }; st() { c -o /dev/null -w "%{http_code}" "$@"; }
FAIL=0; want() { [ "$1" = "$2" ] || { echo "  expected $2 from $3, got $1"; FAIL=1; }; }
echo "== switch to http:80"; c "https://$H/hw/switch/http"; echo; sleep 2
want "$(st "http://$H/hw")" 200 http:80; echo "  https:443 $(st "https://$H/hw") (expect 000)"
echo "== back to https:443 over http"; c "http://$H/hw/switch/https"; echo; sleep 2
want "$(st "https://$H/hw")" 200 https:443
echo "== stalled TLS client and an idle socket open, then switch to https:8443"
nc "$H" 443 < /dev/zero > /dev/null & NC=$!  # never completes a handshake
(exec 3<> "/dev/tcp/$H/443" && sleep 30) 2> /dev/null & IDLE=$!
sleep 2
c "https://$H/hw/switch/https8443"; echo; sleep 3
want "$(st "https://$H:8443/hw")" 200 https:8443
kill $NC $IDLE 2> /dev/null
echo "== 60 requests heap check"; c "https://$H:8443/hw"; for _ in $(seq 60); do st "https://$H:8443/hw" > /dev/null; done
c "https://$H:8443/hw"
echo "== bad key"; c "https://$H:8443/hw/switch/bad"; echo; sleep 3
echo "  https:443 $(st "https://$H/hw") https:8443 $(st "https://$H:8443/hw") (expect 000 000; check the device log)"
[ "$FAIL" = 0 ] || exit 1
