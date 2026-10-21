#!/bin/sh
# write_cases.sh DEVICE_IP KEY [PORT] : drive a device built from templates/udp-write-dut.yaml. Records every datagram
# the device sends to PORT (the udp component's port, default 18511) with udp_listen.py in the working directory,
# presses the eight payload buttons over the API two seconds apart (KEY as for api_button.py, - for a plaintext API),
# sends one 16 byte unicast datagram to the device for the echo case, then compares the recorded datagrams with
# check.py. Exit 1 when a payload is missing or differs; listen.jsonl stays in the working directory.
HERE=$(cd "$(dirname "$0")" && pwd); PY=${PYTHON:-python3}; DEV=$1; KEY=$2; PORT=${3:-18511}
[ -n "$DEV" ] && [ -n "$KEY" ] || { echo "usage: write_cases.sh DEVICE_IP KEY [PORT]"; exit 2; }
ECHO=0102030405060708090a0b0c0d0e0f10
$PY "$HERE/udp_listen.py" "$PORT" 60 "$PWD/listen.jsonl" &
lst=$!
sleep 2
for b in Small Simple Text Empty Stack128 Heap129 Heap600 Lambda300; do
  $PY "$HERE/../api_button.py" "$DEV" "$KEY" press "$b" > /dev/null || echo "press $b failed"; sleep 2
done
$PY -c "import socket; socket.socket(socket.AF_INET, socket.SOCK_DGRAM).sendto(bytes.fromhex('$ECHO'), ('$DEV', $PORT))"
sleep 5
kill $lst 2>/dev/null; wait $lst 2>/dev/null
$PY "$HERE/check.py" "$PWD/listen.jsonl" "$ECHO"
