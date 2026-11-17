#!/bin/bash
# bkab.sh BUILD...: BK72xx SSE A/B, alternating builds. Per pass: serial flash, serial capture, pcap of port 80,
# flow.py (180 s pages, 180 s SSE, REST control), then ssediag.py (60 s pages, 150 s SSE with a REST poll).
# env: PORT, PY, IFACE (default en0; tcpdump needs capture rights)
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$(dirname "$D")"); P=${PORT:?set PORT}; PY=${PY:-python3}
IF=${IFACE:-en0}
. "$D/ltflash.sh"
k=0; FAILED=()
for N in "$@"; do
  k=$((k + 1)); T=$N-p$k
  pkill -f "[s]erial_capture.py $P "; sleep 1
  ltflash bk "$P" "$D/.esphome/build/$N/.pioenvs/$N/firmware.uf2" "$D/$T-upload.log" ||
    { echo "== pass $k $N flash failed three times (see $D/$T-upload.log)"; exit 1; }
  echo "== pass $k $N flashed"
  : > "$D/$T-serial.log"; ($PY "$S/serial_capture.py" "$P" 1200 "$D/$T-serial.log" --released 2> "$D/$T-cap.err" &)
  HOST=$(bash "$S/resolve.sh" "$N" 60) || { echo "no host"; FAILED+=("p$k:no-host"); continue; }; sleep 10
  (tcpdump -i "$IF" -s 128 -w "$D/$T.pcap" host "$HOST" and tcp port 80 > /dev/null 2>&1 &); sleep 1
  $PY -I "$D/flow.py" "$HOST" "$D/$T" | grep -E 'PAGES|SSE|CONTROL|heap'
  [ "${PIPESTATUS[0]}" = 0 ] || FAILED+=("p$k:flow")
  $PY -I "$D/ssediag.py" "$HOST" 150 60 > "$D/$T-diag.txt"
  echo "diag: $(tail -1 "$D/$T-diag.txt"), zero windows $(grep -c 'fast events   0' "$D/$T-diag.txt")," \
    "max lag $(grep -o 'lag [0-9-]*' "$D/$T-diag.txt" | awk '{print $2}' | sort -n | tail -1)"
  pkill -f "[t]cpdump -i $IF -s 128 -w $D/$T.pcap"; pkill -f "[s]erial_capture.py $P "; sleep 1
  [ -s "$D/$T.pcap" ] || { echo "pcap: none written (tcpdump capture rights?)"; FAILED+=("p$k:pcap"); }
  [ -s "$D/$T-serial.log" ] ||
    { echo "serial: EMPTY capture, counts below mean nothing: $(tail -1 "$D/$T-cap.err")"; FAILED+=("p$k:capture"); }
  echo "serial: crash lines $(grep -a -c -E 'CRASH|Crash|abort|HardFault|Undefined' "$D/$T-serial.log")," \
    "boots $(grep -a -c 'Log initialized' "$D/$T-serial.log")"
done
[ ${#FAILED[@]} = 0 ] || { echo "FAILED: ${FAILED[*]}"; exit 1; }
