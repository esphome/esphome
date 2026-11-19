#!/bin/bash
# usage: run.sh LABEL ROUNDS   build both boards, then ROUNDS connect/disconnect rounds through the proxy
# Copy this directory into a scratch directory with a secrets.yaml (api_key, base64 Noise key) and run it there.
# env: TREE (esphome checkout to build the proxy from), PORT (peer devkit serial port), PROXY (proxy address, flashed
# over the air; the first flash of a fresh board goes over USB), PROXY_BT_MAC (the proxy's Bluetooth MAC, printed by
# btdrive.py as bt_mac), PEER_MAC (the peer's Bluetooth MAC, base MAC + 2 on a classic ESP32), SC (path to
# serial_capture.py), PY (checkout python), PEER_TREE (checkout for the peer, defaults to TREE)
D=$(cd "$(dirname "$0")" && pwd); L=${1:?label}; N=${2:-12}; T=${TREE:?set TREE}; PT=${PEER_TREE:-$T}
P=${PORT:?set PORT}; S3=${PROXY:?set PROXY}; SC=${SC:?set SC}; PY=${PY:-$T/venv/bin/python}
PEER=${PEER_MAC:?set PEER_MAC}; CM=${PROXY_BT_MAC:?set PROXY_BT_MAC}
KEY=$(sed -n 's/^api_key: *"\{0,1\}\([^"]*\)"\{0,1\}$/\1/p' "$D/secrets.yaml")
[ -n "$KEY" ] || { echo "FAIL: no api_key in secrets.yaml"; exit 1; }
cd "$D" || exit 1
printf '#pragma once\n#include <cstdint>\nstatic const uint8_t CENTRAL_BDA[6] = {0x%s};\n' "${CM//:/, 0x}" > central_bda.hpp
(cd "$PT" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome run --no-logs "$D/btpeer.yaml" --device "$P" > "$D/$L-peer-upload.log" 2>&1) \
  || { echo "FAIL: peer build or upload failed"; exit 1; }
(cd "$T" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome run --no-logs "$D/btproxy.yaml" --device "$S3" > "$D/$L-proxy-upload.log" 2>&1) \
  || { echo "FAIL: proxy build or upload failed"; exit 1; }
SECS=$((N * 26 + 40))
pkill -f "[s]erial_capture.py $P"
$PY "$SC" "$P" $SECS "$D/$L-peer.txt" --ms > /dev/null 2>&1 & CAP=$!
(cd "$T" && PYTHONUNBUFFERED=1 exec timeout $((SECS - 10)) $PY -m esphome logs "$D/btproxy.yaml" --device "$S3" > "$D/$L-proxy.txt" 2>&1) & LOGS=$!
trap 'kill $CAP $LOGS 2>/dev/null' EXIT
sleep 6
$PY "$D/btdrive.py" "$S3" "$KEY" "$PEER" "$N" 10 > "$D/$L-drive.txt" 2>&1; RC=$?
sleep 10
kill $CAP $LOGS 2>/dev/null; wait 2>/dev/null; trap - EXIT
[ -s "$D/$L-peer.txt" ] || { tail -3 "$D/$L-drive.txt"; echo "FAIL: peer capture got nothing"; exit 1; }
$PY "$D/an.py" "$D" "$L" "$N"; ARC=$?
[ "$RC" = 0 ] || { tail -1 "$D/$L-drive.txt"; echo "FAIL: driver rc $RC (not every round completed)"; exit 1; }
exit $ARC
