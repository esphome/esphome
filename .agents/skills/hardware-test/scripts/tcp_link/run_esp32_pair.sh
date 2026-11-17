#!/bin/bash
# ESP32 peer (tcpe1) against host peer tcph3 and the raw Python peer
D=$(cd "$(dirname "$0")" && pwd); cd "$D" || exit 1; B=$D/.esphome/build; P=python3; R=e; E=$(cat .eip);
ts() { echo "$(date +%T) $*" >> $R-events.txt; }
: > $R-events.txt
(API_KEY=${API_KEY:-$(cat apikey.txt)} exec ${PY:-python3} apiwatch.py $E 6053 200 > $R-apiE.txt 2>&1) &
(exec $P -I tcp_peer.py server 46504 --duration 50 --silent-after 8 > $R-ps-silent.txt 2>&1) &
ts api+silent server
sleep 10
(exec $B/tcph3/.pioenvs/tcph3/program > $R-h3a.txt 2>&1 < /dev/null) & H=$!
ts start h3
sleep 55; kill -STOP $H; ts STOP h3
sleep 15; kill -CONT $H; ts CONT h3
sleep 20; kill -9 $H; ts KILL h3
sleep 7
(exec timeout 300 $B/tcph3/.pioenvs/tcph3/program > $R-h3b.txt 2>&1 < /dev/null) &
ts restart h3
(exec $P -I tcp_peer.py server 46504 --duration 20 --oversize-after 3 1100 > $R-ps-over.txt 2>&1) &
sleep 12
(exec $P -I tcp_peer.py client $E 46503 --duration 30 --once --hold 15 --send 7 > $R-pcA.txt 2>&1) &
ts client A
sleep 5
(exec $P -I tcp_peer.py client $E 46503 --duration 25 --once --send 5,1022 > $R-pcB.txt 2>&1) &
ts client B
sleep 25
ts client C to deny_srv
$P -I tcp_peer.py client $E 46505 --duration 6 --once > $R-pcC.txt 2>&1
sleep 2
ts client D oversize
$P -I tcp_peer.py client $E 46503 --duration 10 --once --oversize-after 2 1100 > $R-pcD.txt 2>&1
ts done
