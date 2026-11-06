#!/bin/bash
# t2.sh HOST: switch the listener at run time: to http:80 and back, to https:8443 with a stalled TLS client open
# (stall.py covers idle sockets), 60 requests for a heap check, then a key that does not match the certificate (the
# server must log the error and stay off, with no fallback to http:80). PASS when every listener answered or stayed
# off as expected, FAIL otherwise.
H=${1:?device address}; c() { curl -sk --max-time 10 "$@"; }; st() { c -o /dev/null -w "%{http_code}" "$@"; }
FAIL=0; want() { [ "$1" = "$2" ] || { echo "  expected $2 from $3, got $1"; FAIL=1; }; }
echo "== switch to http:80"; c "https://$H/hw/switch/http"; echo; sleep 2
want "$(st "http://$H/hw")" 200 http:80; want "$(st "https://$H/hw")" 000 https:443
echo "== back to https:443 over http"; c "http://$H/hw/switch/https"; echo; sleep 2
want "$(st "https://$H/hw")" 200 https:443
echo "== stalled TLS client open, then switch to https:8443"
# a TLS handshake record header with no body: the server waits for the rest; exits early if the server closes it
python3 -c 'import socket, sys
s = socket.create_connection((sys.argv[1], 443), timeout=60)
s.sendall(b"\x16\x03\x01\x02\x00")
sys.exit(3 if s.recv(1) == b"" else 0)' "$H" 2> /dev/null & NC=$!
sleep 2
kill -0 $NC 2> /dev/null || { echo "  the stalled client was closed before the switch"; FAIL=1; }
c "https://$H/hw/switch/https8443"; echo; sleep 3
want "$(st "https://$H:8443/hw")" 200 https:8443
kill $NC 2> /dev/null
echo "== 60 requests heap check"; c "https://$H:8443/hw"; for _ in $(seq 60); do st "https://$H:8443/hw" > /dev/null; done
c "https://$H:8443/hw"
echo "== bad key"; c "https://$H:8443/hw/switch/bad"; echo; sleep 3
want "$(st "https://$H/hw")" 000 https:443; want "$(st "https://$H:8443/hw")" 000 https:8443
want "$(st "http://$H/hw")" 000 http:80; echo "  check the device log for the key error"
[ "$FAIL" = 0 ] && echo PASS || { echo "FAIL: a listener did not answer or stay off as expected"; exit 1; }
