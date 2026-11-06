#!/bin/bash
# cases.sh PROXY_HOST LABEL: half-open API session cases against a proxy that already runs the build under test
# (proxy.yaml) and the peripheral (peripheral.yaml). Captures both serial logs with millisecond stamps into
# out-ho<LABEL>/ and runs:
#   C  owner client killed (socket closed), new client after 4 s, watched 60 s
#   H  owner frozen with SIGSTOP (socket stays open, no FIN), new client after 4 s, watched 300 s, then it asks the
#      proxy to connect to the peripheral and releases it
#   N  owner frozen and nobody reconnects for 200 s, then watched 40 s
# env: PROXY_PORT, PERI_PORT (serial ports), SC (path to serial_capture.py), PY (python with aioesphomeapi),
# API_KEY (the proxy's api key, - for plaintext)
D=$(cd "$(dirname "$0")" && pwd); H=${1:?proxy host}; L=${2:?label}; PY=${PY:-python3}; SC=${SC:?set SC}
PP=${PROXY_PORT:?set PROXY_PORT}; QP=${PERI_PORT:?set PERI_PORT}; KEY=${API_KEY:?set API_KEY}
O=$D/out-ho$L; rm -rf "$O"; mkdir -p "$O"
pkill -f "[s]erial_capture.py $PP"; pkill -f "[s]erial_capture.py $QP"
$PY "$SC" "$PP" 900 "$O/proxy.txt" --ms > /dev/null 2>&1 & C1=$!
$PY "$SC" "$QP" 900 "$O/peri.txt" --ms > /dev/null 2>&1 & C2=$!
trap 'kill $C1 $C2 2>/dev/null; [ -n "$OWNER" ] && { kill -9 $OWNER; kill -CONT $OWNER; } 2>/dev/null' EXIT
sleep 3
FAIL=0
own() {  # own CASE: start the owner and wait until it holds the link
  $PY "$D/ha.py" "$H" "$KEY" owner > "$O/owner-$1.log" 2>&1 & OWNER=$!
  for _ in $(seq 60); do grep -q "OWNER HOLDS LINK" "$O/owner-$1.log" && break; sleep 1; done
  grep -q "OWNER HOLDS LINK" "$O/owner-$1.log" || { echo "FAIL: case $1: owner never connected"; FAIL=1; kill -9 $OWNER; return 1; }
  ADDR=$(grep -oE "peripheral 0x[0-9a-f]+" "$O/owner-$1.log" | awk '{print $2}')
}
release() { kill -9 $OWNER; kill -CONT $OWNER 2>/dev/null; OWNER=; }
echo "CASE C $(date +%T)"
own C && { sleep 5; kill -9 $OWNER; OWNER=; sleep 4; $PY "$D/ha.py" "$H" "$KEY" observe 60 > "$O/obs-C.log" 2>&1; }
sleep 10
echo "CASE H $(date +%T)"
own H && { sleep 5; kill -STOP $OWNER; sleep 4; $PY "$D/ha.py" "$H" "$KEY" observe 300 connect "$ADDR" > "$O/obs-H.log" 2>&1; release; }
sleep 10
echo "CASE N $(date +%T)"
own N && { sleep 5; kill -STOP $OWNER; sleep 200; $PY "$D/ha.py" "$H" "$KEY" observe 40 > "$O/obs-N.log" 2>&1; release; }
for c in C H N; do
  f=$O/obs-$c.log; [ -s "$f" ] || continue
  echo "$c: first advert $(grep -c 'FIRST ADVERT' "$f"), last count '$(grep 'adverts so far' "$f" | tail -1 | sed 's/.*so far //')'," \
    "$(grep -o 'state connected=[A-Za-z]*.*after [0-9.]* s' "$f" | head -1)"
done
echo "proxy: replaces warnings $(grep -a -c replaces "$O/proxy.txt"), resets $(grep -a -c -E 'Guru|abort\(\)|rst:0x' "$O/proxy.txt")"
[ "$FAIL" = 0 ] || exit 1
