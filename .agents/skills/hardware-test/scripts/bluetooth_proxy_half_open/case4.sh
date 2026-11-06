#!/bin/bash
# case4.sh PROXY_HOST LABEL: case H three times, with the new client asking for the held peripheral right after it
# subscribes (ha.py swap) and retrying on the first advert. Logs go to out-ho<LABEL>-4/. env as for cases.sh.
D=$(cd "$(dirname "$0")" && pwd); H=${1:?proxy host}; L=${2:?label}; PY=${PY:-python3}; SC=${SC:?set SC}
PP=${PROXY_PORT:?set PROXY_PORT}; QP=${PERI_PORT:?set PERI_PORT}; KEY=${API_KEY:?set API_KEY}
O=$D/out-ho$L-4; rm -rf "$O"; mkdir -p "$O"
pkill -f "[s]erial_capture.py $PP"; pkill -f "[s]erial_capture.py $QP"
$PY "$SC" "$PP" 150 "$O/proxy.txt" --ms > /dev/null 2>&1 & C1=$!
$PY "$SC" "$QP" 150 "$O/peri.txt" --ms > /dev/null 2>&1 & C2=$!
trap 'kill $C1 $C2 2>/dev/null' EXIT
sleep 3
FAIL=0
for r in 1 2 3; do
  $PY "$D/ha.py" "$H" "$KEY" owner > "$O/owner-$r.log" 2>&1 & OWNER=$!
  for _ in $(seq 60); do grep -q "OWNER HOLDS LINK" "$O/owner-$r.log" && break; sleep 1; done
  if ! grep -q "OWNER HOLDS LINK" "$O/owner-$r.log"; then echo "FAIL: round $r: owner never connected"; FAIL=1; kill -9 $OWNER; continue; fi
  ADDR=$(grep -oE "peripheral 0x[0-9a-f]+" "$O/owner-$r.log" | awk '{print $2}')
  sleep 5; kill -STOP $OWNER; sleep 4
  $PY "$D/ha.py" "$H" "$KEY" swap "$ADDR" > "$O/swap-$r.log" 2>&1
  kill -9 $OWNER; kill -CONT $OWNER 2>/dev/null
  echo "round $r: $(grep -o 'CONNECTED on attempt [0-9] at [0-9.]* s' "$O/swap-$r.log" || echo 'never connected')"
  grep -q 'CONNECTED on attempt' "$O/swap-$r.log" || FAIL=1
  sleep 15
done
[ "$FAIL" = 0 ] || exit 1
