#!/bin/zsh
# sseabc.sh HOST TAG : 40 s SSE streams with (A) the API soak, (B) page loads every 5 s, (C) nothing else; state events per 5 s
D=${0:A:h}; H=$1; T=$2; PY=${PY:?set PY}
ts() { while IFS= read -r l; do echo "$(date +%s) $l"; done; }
bins() { awk 'NR==1{t0=$1} /event: state/ {c[int(($1-t0)/5)]++} END {for(i=0;i<8;i++) printf "%d ", c[i]; print ""}' $1; }
(curl -s -N -m 40 http://$H/events | ts > $D/$T-A.txt) & API_KEY=$(cat $D/apikey.txt) $PY -I $D/soak.py $H 40 $D/$T-A-soak.log > /dev/null; wait
echo "A sse+api   states $(grep -c 'event: state' $D/$T-A.txt) per5s: $(bins $D/$T-A.txt)"
(curl -s -N -m 40 http://$H/events | ts > $D/$T-B.txt) & (for i in 1 2 3 4 5 6 7; do curl -s -o /dev/null -m 10 http://$H/; sleep 5; done); wait
echo "B sse+pages states $(grep -c 'event: state' $D/$T-B.txt) per5s: $(bins $D/$T-B.txt)"
curl -s -N -m 40 http://$H/events | ts > $D/$T-C.txt
echo "C sse alone states $(grep -c 'event: state' $D/$T-C.txt) per5s: $(bins $D/$T-C.txt)"
