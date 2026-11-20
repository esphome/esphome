#!/bin/bash
# run_pair.sh host|esp32 [RUN]: the tcp link timeline against one device under test (boot states, SIGSTOP/SIGCONT of
# the host peer, kill -9 and restart, silent and oversize raw servers, one client at a time, a refused client, oversize
# from a client). Logs go to <RUN>-*.txt (RUN defaults to the mode), with ANSI colours stripped. Exits 1 when a job ends
# with the wrong status, or the device logs show bad raw packets, an accepted 1023 byte send or a lost probe packet.
#   host:  the device is host build tcph1 here, its peer tcph2; the refused client connects from the computer's LAN
#          address (LAN, macOS `ipconfig getifaddr en0` otherwise).
#   esp32: the device is the ESP32 board NAME (default tcpe1, found with resolve.sh), its peer host build tcph3; the
#          refused client connects to its deny_srv. The board's log is taken over the API into <RUN>-dut.txt.
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
for b in $( [ "$M" = host ] && echo tcph1) $PEER; do
  [ -x "$B/$b/.pioenvs/$b/program" ] || { echo "build $b here first"; exit 1; }
done
API_KEY=${API_KEY:-$(cat apikey.txt 2>/dev/null)}
export API_KEY=${API_KEY:?set API_KEY or write apikey.txt}
ts() { echo "$(date +%T) $*" >> "$R-events.txt"; }
JOBS=(); FAIL=0
job() { # NAME EXIT CMD...: run CMD in the background into <RUN>-NAME.txt; its exit status must be EXIT at the end
  local name=$1 want=$2; shift 2
  (exec "$@" > "$R-$name.txt" 2>&1 < /dev/null) &
  JOBS+=("$name:$want:$!")
}
run() { # NAME CMD...: run CMD into <RUN>-NAME.txt, which must exit 0
  local name=$1; shift
  "$@" > "$R-$name.txt" 2>&1 < /dev/null || { echo "JOB FAILED $name exit=$?"; FAIL=1; }
}
sig() { kill "-$1" "$H" || { echo "kill -$1 $PEER failed: it is not running"; FAIL=1; }; ts "$1" $PEER; }
: > "$R-events.txt"
if [ "$M" = host ]; then
  job dut 124 timeout 140 "$B/tcph1/.pioenvs/tcph1/program"
  ts start tcph1
fi
# The board's own log comes over the API in esp32 mode; a host build logs to its stdout
DUTLOG=(); [ "$M" = esp32 ] && DUTLOG=("$R-dut.txt")
job api 0 ${PY:-python3} apiwatch.py "$E" "$API_PORT" $WATCH "${DUTLOG[@]}"
job ps-silent 0 $P -I tcp_peer.py server "$PROBE_CLI_PORT" --duration $SILENT --silent-after 8
ts api and silent server
sleep $WARM
job peer-a 137 "$B/$PEER/.pioenvs/$PEER/program"; H=$!
ts start $PEER
sleep $RUNS; sig STOP
sleep 15; sig CONT
sleep $HOLD; sig 9
sleep 7
job peer-b 124 timeout 70 "$B/$PEER/.pioenvs/$PEER/program"
ts restart $PEER
sleep 1
job ps-over 0 $P -I tcp_peer.py server "$PROBE_CLI_PORT" --duration 20 --oversize-after 3 1100
ts probe server oversize
sleep 12
job pcA 0 $P -I tcp_peer.py client "$E" "$PROBE_PORT" --duration 30 --once --hold 15 --send 7
ts client A
sleep 5
job pcB 0 $P -I tcp_peer.py client "$E" "$PROBE_PORT" --duration 25 --once --send 5,1022
ts client B
sleep 25
ts client C refused
run pcC $P -I tcp_peer.py "${REFUSED[@]}" --duration 6 --once --expect-reject
sleep 2
ts client D oversize
run pcD $P -I tcp_peer.py client "$E" "$PROBE_PORT" --duration 10 --once --oversize-after 2 1100
ts done
for j in "${JOBS[@]}"; do
  IFS=: read -r name want pid <<< "$j"
  wait "$pid"; rc=$?
  [ "$rc" = "$want" ] || { echo "JOB FAILED $name exit=$rc (expected $want), see $R-$name.txt"; FAIL=1; }
done
perl -pi -e 's/\e\[[0-9;]*m//g' "$R"-*.txt
# Every device log, the device under test's first: the raw streams carried no bad packet, and the 1023 byte send was
# refused while connected
for f in "$R-dut.txt" "$R-peer-a.txt" "$R-peer-b.txt"; do
  st=$(grep -a 'STAT raw_srv' "$f" | tail -1)
  if [ -z "$st" ]; then
    echo "CHECK FAILED $f: no STAT line"; FAIL=1
  elif ! grep -Eq 'raw_srv .* bad=0 gap=.*raw_cli .* bad=0 gap=' <<< "$st"; then
    echo "CHECK FAILED $f: bad raw packets: $st"; FAIL=1
  fi
  n=$(grep -a -c -E 'bad payload|short packet|oversize packet' "$f")
  [ "$n" = 0 ] || { echo "CHECK FAILED $f: $n bad packet warnings"; FAIL=1; }
  ! grep -a -q '1023 byte send returned 1' "$f" || { echo "CHECK FAILED $f: the 1023 byte send was accepted"; FAIL=1; }
  grep -a -q '1023 byte send returned 0 connected=1' "$f" || echo "note $f: no 1023 byte send while connected"
done
# Every checked size the raw peer clients sent reached the device's probe server intact
for f in "$R-pcA.txt" "$R-pcB.txt"; do
  while read -r size sum; do
    grep -a -q "probe_srv RX size=$size sum=$sum" "$R-dut.txt" ||
      { echo "CHECK FAILED $f: size $size sum $sum never reached probe_srv"; FAIL=1; }
  done < <(grep -a 'SENT size' "$f" | sed -E 's/.*SENT size ([0-9]+) sum ([0-9]+).*/\1 \2/')
done
echo "RUN $R done failed=$FAIL"
exit $FAIL
