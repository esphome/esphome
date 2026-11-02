#!/bin/bash
# usage: run-ap.sh LABEL   the DUT runs the fallback AP on 10.42.7.1, the other board joins it and fetches its page
# Copy this directory into a scratch directory and run it there; no secrets are needed.
# env: TREE (esphome checkout for the DUT), CLIENT_TREE (checkout for the client, default TREE), DUT_PORT, DUT_PLAT,
# CLIENT_PORT, CLIENT_PLAT (esp32 or esp8266), SC (path to serial_capture.py), PY (default $TREE/venv/bin/python)
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; CT=${CLIENT_TREE:-$T}
PY=${PY:-$T/venv/bin/python}; SC=${SC:?set SC}; DP=${DUT_PORT:?set DUT_PORT}; CP=${CLIENT_PORT:?set CLIENT_PORT}
N=mip-ap-$L; CN=mip-client
up() { (cd "$1" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome -s name "$2" -s plat "$3" run --no-logs "$D/$4" \
  --device "$5" > "$D/$2-upload.log" 2>&1) || { echo "FAIL: $2 build or upload"; exit 1; }; }
up "$T" "$N" "${DUT_PLAT:-esp32}" ap.yaml "$DP"
up "$CT" "$CN" "${CLIENT_PLAT:-esp8266}" client.yaml "$CP"
rm -f "$D/$N-serial.txt" "$D/$CN-$N-serial.txt"
$PY "$SC" "$DP" 120 "$D/$N-serial.txt" --reset > /dev/null 2>&1 & C1=$!
$PY "$SC" "$CP" 120 "$D/$CN-$N-serial.txt" --reset > /dev/null 2>&1 & C2=$!
wait $C1; R1=$?; wait $C2; R2=$?
strip() { LC_ALL=C sed 's/\x1b\[[0-9;]*m//g' | sort -u -k2; }
echo "--- DUT"; LC_ALL=C grep -a -E "Manual IP|Starting fallback|IP Address|Setting up AP|rst cause|Guru|abort|panic" \
  "$D/$N-serial.txt" | strip | head -8
echo "--- client"; LC_ALL=C grep -a -E "IP Address:|Gateway:|Subnet:" "$D/$CN-$N-serial.txt" | strip | head -4
ok=$(grep -a -c 'status=200' "$D/$CN-$N-serial.txt"); bad=$(grep -a -c 'GET http://10.42.7.1/ failed' "$D/$CN-$N-serial.txt")
echo "client GET ok=$ok failed=$bad"
fail=0
[ "$R1" = 0 ] && [ -s "$D/$N-serial.txt" ] || { echo "FAIL: DUT capture failed or empty (rc $R1)"; fail=1; }
[ "$R2" = 0 ] && [ -s "$D/$CN-$N-serial.txt" ] || { echo "FAIL: client capture failed or empty (rc $R2)"; fail=1; }
grep -a -q -E "IP Address: +10\.42\.7\.1\b" "$D/$N-serial.txt" || { echo "FAIL: AP address is not 10.42.7.1"; fail=1; }
grep -a -q -E "IP Address: +10\.42\.7\." "$D/$CN-$N-serial.txt" || { echo "FAIL: client never joined"; fail=1; }
[ "$ok" -gt 0 ] || { echo "FAIL: no page fetched"; fail=1; }
grep -a -q -E "Guru|abort\(\)|panic|[eE]xception \(|Soft WDT reset|wdt reset" "$D/$N-serial.txt" && { echo "FAIL: DUT crash"; fail=1; }
[ $fail = 0 ] && echo PASS; exit $fail
