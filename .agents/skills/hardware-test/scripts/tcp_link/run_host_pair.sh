#!/bin/bash
# Host to host scenario run on one computer (macOS: LAN from `ipconfig getifaddr en0`, or set LAN): boot state, peer pause, peer kill and restart, raw peer probes
D=$(cd "$(dirname "$0")" && pwd); cd "$D" || exit 1; B=$D/.esphome/build; P=python3; R=$1; LAN=${LAN:-$(ipconfig getifaddr en0)}
ts() { echo "$(date +%T) $*" >> $R-events.txt; }
: > $R-events.txt
(exec timeout 140 $B/tcph1/.pioenvs/tcph1/program > $R-h1.txt 2>&1 < /dev/null) & H1=$!
ts start h1
(API_KEY=${API_KEY:-$(cat apikey.txt)} exec ${PY:-python3} apiwatch.py 127.0.0.1 46151 135 > $R-api1.txt 2>&1) &
(exec $P -I tcp_peer.py server 46104 --duration 58 --silent-after 8 > $R-ps-silent.txt 2>&1) &
sleep 8
(exec $B/tcph2/.pioenvs/tcph2/program > $R-h2a.txt 2>&1 < /dev/null) & H2=$!
ts start h2
sleep 17; kill -STOP $H2; ts STOP h2
sleep 15; kill -CONT $H2; ts CONT h2
sleep 15; kill -9 $H2; ts KILL h2
sleep 7
(exec timeout 70 $B/tcph2/.pioenvs/tcph2/program > $R-h2b.txt 2>&1 < /dev/null) &
ts restart h2
sleep 1
(exec $P -I tcp_peer.py server 46104 --duration 20 --oversize-after 3 1100 > $R-ps-over.txt 2>&1) &
ts probe server oversize
sleep 12
(exec $P -I tcp_peer.py client 127.0.0.1 46103 --duration 30 --once --hold 15 --send 7 > $R-pcA.txt 2>&1) &
ts client A
sleep 5
(exec $P -I tcp_peer.py client 127.0.0.1 46103 --duration 25 --once --send 5,1022 > $R-pcB.txt 2>&1) &
ts client B
sleep 25
ts client C from LAN address
$P -I tcp_peer.py client $LAN 46103 --bind $LAN --duration 6 --once > $R-pcC.txt 2>&1
sleep 2
ts client D oversize
$P -I tcp_peer.py client 127.0.0.1 46103 --duration 10 --once --oversize-after 2 1100 > $R-pcD.txt 2>&1
ts done
wait
for f in $R-h1.txt $R-h2a.txt $R-h2b.txt; do sed 's/\x1b\[[0-9;]*m//g' $f > ${f%.txt}c.txt; done
