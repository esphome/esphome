#!/bin/bash
# cases.sh PROXY_HOST LABEL [swap]: half-open API session cases against a proxy that already runs the build under
# test (proxy.yaml) and the peripheral (peripheral.yaml). Captures both serial logs with millisecond stamps into
# out-ho<LABEL>/ and runs:
#   C  owner client killed (socket closed), new client after 4 s, watched 60 s
#   H  owner frozen with SIGSTOP (socket stays open, no FIN), new client after 4 s, watched 300 s, then it asks the
#      proxy to connect to the peripheral and releases it
#   N  owner frozen and nobody reconnects for 200 s, then watched 40 s
# With swap it instead runs case H three times, the new client asking for the held peripheral right after it
# subscribes (ha.py swap) and retrying on the first advert, with logs in out-ho<LABEL>-4/. PASS when every case's
# observer saw the peripheral advertise (the held link was released) or every swap round connected.
# env: PROXY_PORT, PERI_PORT (serial ports), SC (path to serial_capture.py), PY (python with aioesphomeapi),
# API_KEY (the proxy's api key, - for plaintext)
D=$(cd "$(dirname "$0")" && pwd); H=${1:?proxy host}; L=${2:?label}; MODE=${3:-cases}; PY=${PY:-python3}
SC=${SC:?set SC}
PP=${PROXY_PORT:?set PROXY_PORT}; QP=${PERI_PORT:?set PERI_PORT}; KEY=${API_KEY:?set API_KEY}
case $MODE in
  cases) O=$D/out-ho$L; CAP_S=900;;
  swap) O=$D/out-ho$L-4; CAP_S=150;;
  *) echo "the third argument is swap or nothing"; exit 2;;
esac
rm -rf "$O"; mkdir -p "$O"
pkill -f "[s]erial_capture.py $PP"; pkill -f "[s]erial_capture.py $QP"
$PY "$SC" "$PP" $CAP_S "$O/proxy.txt" --ms > /dev/null 2>&1 & C1=$!
$PY "$SC" "$QP" $CAP_S "$O/peri.txt" --ms > /dev/null 2>&1 & C2=$!
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
observe() {  # observe CASE ARGS...: run the observer, keeping its exit status in obs-CASE.rc
  local c=$1; shift
  $PY "$D/ha.py" "$H" "$KEY" observe "$@" > "$O/obs-$c.log" 2>&1; echo $? > "$O/obs-$c.rc"
}
if [ "$MODE" = swap ]; then
  for r in 1 2 3; do
    own "$r" || continue
    sleep 5; kill -STOP $OWNER; sleep 4
    $PY "$D/ha.py" "$H" "$KEY" swap "$ADDR" > "$O/swap-$r.log" 2>&1
    release
    echo "round $r: $(grep -o 'CONNECTED on attempt [0-9] at [0-9.]* s' "$O/swap-$r.log" || echo 'never connected')"
    grep -q 'CONNECTED on attempt' "$O/swap-$r.log" || FAIL=1
    sleep 15
  done
  [ "$FAIL" = 0 ] && echo PASS || { echo "FAIL: a round never connected"; exit 1; }
  exit 0
fi
echo "CASE C $(date +%T)"
own C && { sleep 5; kill -9 $OWNER; OWNER=; sleep 4; observe C 60; }
sleep 10
echo "CASE H $(date +%T)"
own H && { sleep 5; kill -STOP $OWNER; sleep 4; observe H 300 connect "$ADDR"; release; }
sleep 10
echo "CASE N $(date +%T)"
own N && { sleep 5; kill -STOP $OWNER; sleep 200; observe N 40; release; }
for c in C H N; do
  f=$O/obs-$c.log; [ -s "$f" ] || { echo "$c: no observer run"; FAIL=1; continue; }
  rc=$(cat "$O/obs-$c.rc"); first=$(grep -c 'FIRST ADVERT' "$f")
  echo "$c: observer exit $rc, first advert $first, last count '$(grep 'adverts so far' "$f" | tail -1 | sed 's/.*so far //')'," \
    "$(grep -o 'state connected=[A-Za-z]*.*after [0-9.]* s' "$f" | head -1)"
  [ "$rc" = 0 ] && [ "$first" -gt 0 ] || FAIL=1
done
R=$(grep -a -c -E 'Guru|abort\(\)|Backtrace|rst:0x' "$O/proxy.txt")
echo "proxy: replaces warnings $(grep -a -c replaces "$O/proxy.txt"), resets $R"
[ "$R" = 0 ] || FAIL=1
[ "$FAIL" = 0 ] && echo PASS || { echo "FAIL: an observer failed or never saw the peripheral advertise, or the proxy reset"; exit 1; }
