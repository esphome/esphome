#!/bin/bash
# run_pair.sh host|esp32 [RUN]: the tcp link timeline against one device under test (boot states, SIGSTOP/SIGCONT of
# the host peer, kill -9 and restart, silent and oversize raw servers, one client at a time, a refused client, oversize
# from a client). Logs go to <RUN>-*.txt (RUN defaults to the mode), with ANSI colours stripped.
#   host:  the device is host build tcph1 here, its peer tcph2; the refused client connects from the computer's LAN
#          address (LAN, macOS `ipconfig getifaddr en0` otherwise).
#   esp32: the device is the ESP32 board NAME (default tcpe1, found with resolve.sh), its peer host build tcph3; the
#          refused client connects to its deny_srv.
# Build the host configs here first. env: API_KEY (apikey.txt otherwise), PY, and the ports of the example configs:
# API_PORT, PROBE_PORT, PROBE_CLI_PORT, DENY_PORT.
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D"); cd "$D" || exit 1; B=$D/.esphome/build; P=python3
M=${1:?host or esp32}; R=${2:-$M}
if [ "$M" = host ]; then
  E=127.0.0.1; PEER=tcph2; API_PORT=${API_PORT:-46151}; PROBE_PORT=${PROBE_PORT:-46103}
  PROBE_CLI_PORT=${PROBE_CLI_PORT:-46104}; LAN=${LAN:-$(ipconfig getifaddr en0 2>/dev/null)}
  : "${LAN:?set LAN to the LAN address of this computer}"
  WARM=8; RUNS=17; HOLD=15; WATCH=135; SILENT=58
  REFUSED=(client "$LAN" "$PROBE_PORT" --bind "$LAN")
else
  E=$(bash "$S/resolve.sh" "${NAME:-tcpe1}") || exit 1; PEER=tcph3; API_PORT=${API_PORT:-6053}
  PROBE_PORT=${PROBE_PORT:-46503}; PROBE_CLI_PORT=${PROBE_CLI_PORT:-46504}
  WARM=10; RUNS=55; HOLD=20; WATCH=200; SILENT=50
  REFUSED=(client "$E" "${DENY_PORT:-46505}")
fi
ts() { echo "$(date +%T) $*" >> "$R-events.txt"; }
: > "$R-events.txt"
if [ "$M" = host ]; then
  (exec timeout 140 "$B/tcph1/.pioenvs/tcph1/program" > "$R-dut.txt" 2>&1 < /dev/null) &
  ts start tcph1
fi
(API_KEY=${API_KEY:-$(cat apikey.txt)} exec ${PY:-python3} apiwatch.py "$E" "$API_PORT" $WATCH > "$R-api.txt" 2>&1) &
(exec $P -I tcp_peer.py server "$PROBE_CLI_PORT" --duration $SILENT --silent-after 8 > "$R-ps-silent.txt" 2>&1) &
ts api and silent server
sleep $WARM
(exec "$B/$PEER/.pioenvs/$PEER/program" > "$R-peer-a.txt" 2>&1 < /dev/null) & H=$!
ts start $PEER
sleep $RUNS; kill -STOP $H; ts STOP $PEER
sleep 15; kill -CONT $H; ts CONT $PEER
sleep $HOLD; kill -9 $H; ts KILL $PEER
sleep 7
(exec timeout 70 "$B/$PEER/.pioenvs/$PEER/program" > "$R-peer-b.txt" 2>&1 < /dev/null) &
ts restart $PEER
sleep 1
(exec $P -I tcp_peer.py server "$PROBE_CLI_PORT" --duration 20 --oversize-after 3 1100 > "$R-ps-over.txt" 2>&1) &
ts probe server oversize
sleep 12
(exec $P -I tcp_peer.py client "$E" "$PROBE_PORT" --duration 30 --once --hold 15 --send 7 > "$R-pcA.txt" 2>&1) &
ts client A
sleep 5
(exec $P -I tcp_peer.py client "$E" "$PROBE_PORT" --duration 25 --once --send 5,1022 > "$R-pcB.txt" 2>&1) &
ts client B
sleep 25
ts client C refused
$P -I tcp_peer.py "${REFUSED[@]}" --duration 6 --once > "$R-pcC.txt" 2>&1
sleep 2
ts client D oversize
$P -I tcp_peer.py client "$E" "$PROBE_PORT" --duration 10 --once --oversize-after 2 1100 > "$R-pcD.txt" 2>&1
ts done
wait
perl -pi -e 's/\e\[[0-9;]*m//g' "$R"-*.txt
