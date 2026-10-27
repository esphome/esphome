#!/bin/bash
# usage: run-sta.sh LABEL   flash sta.yaml, capture 75 s, ping the manual address, check the dump and the DNS probe
# Copy this directory into a scratch directory with a secrets.yaml (wifi_ssid, wifi_password) and run it there.
# env: TREE (esphome checkout), PORT (board serial port), PLAT (esp32 or esp8266), IP (the board's own DHCP lease),
# GATEWAY, SUBNET, DNS1 (the network's values), SC (path to serial_capture.py), PY (default $TREE/venv/bin/python)
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}
P=${PORT:?set PORT}; IP=${IP:?set IP}; SC=${SC:?set SC}; N=mip-sta-$L; C=$D/$N-serial.txt
(cd "$T" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome -s name "$N" -s plat "${PLAT:-esp32}" -s ip "$IP" \
  -s gateway "${GATEWAY:?set GATEWAY}" -s subnet "${SUBNET:?set SUBNET}" -s dns1 "${DNS1:?set DNS1}" \
  upload "$D/sta.yaml" --device "$P" > "$D/$N-upload.log" 2>&1) || { echo "FAIL: build or upload"; exit 1; }
$PY "$SC" "$P" 75 "$C" --reset > /dev/null 2>&1 & CAP=$!
# ping exits 0 when at least one reply came back, on macOS and Linux alike.
sleep 50; PING=$(ping -c 10 -i 0.3 "$IP" 2>&1); PING_RC=$?; wait $CAP
LC_ALL=C grep -a -E "Manual IP|IP Address:|Gateway:|Subnet:|DNS1:|DNS2:|dnsprobe|rst cause|Guru|abort|panic" "$C" \
  | LC_ALL=C sed 's/\x1b\[[0-9;]*m//g' | sort -u -k2 | head -14
echo "ping: $(echo "$PING" | grep -E 'transmitted')"
fail=0
grep -a -q -E "dns0=$DNS1 dns1=9\.9\.9\.9 lookup=ok" "$C" || { echo "FAIL: DNS servers or lookup"; fail=1; }
grep -a -q -E "IP Address: +$IP\b" "$C" || { echo "FAIL: address is not $IP"; fail=1; }
[ $PING_RC = 0 ] || { echo "FAIL: no ping reply"; fail=1; }
grep -a -q -E "Guru|abort\(\)|panic" "$C" && { echo "FAIL: crash"; fail=1; }
[ $fail = 0 ] && echo PASS; exit $fail
