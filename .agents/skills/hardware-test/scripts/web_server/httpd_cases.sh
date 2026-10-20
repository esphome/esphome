#!/bin/sh
# httpd_cases.sh HOST USER PASS LABEL CAPTURE : HTTPD task stack load for the web_server_idf task size; probe lines from CAPTURE
H=$1; U=$2; P=$3; LABEL=$4; LOG=$5; HERE=$(dirname "$0")
probes() { tail -n +$(($1+1)) "$LOG" | sed 's/\x1b\[[0-9;]*m//g' | grep -a -o "\[[0-9:]*\]\[I\]\[probe:[0-9]*\]: .*httpd min free stack [0-9]*\|\[[0-9:]*\]\[[WE]\]\[[a-z_.]*:[0-9]*\].*\|Guru.*" | grep -a -v "took a long time\|does not fit\|web_server_idf:195" | sed 's/\[I\]\[probe:[0-9]*\]: loopTask min free stack \([0-9]*\), httpd min free stack \([0-9]*\)/ loop \1 httpd \2/' | uniq -f1 | cut -c1-140; }
step() { n0=$(wc -l < "$LOG"); echo "=== $LABEL: $1 ($(date +%T))"; shift; "$@"; echo "--- device log:"; probes "$n0"; }
step "idle 15 s" sleep 15
step "request mix, 20 rounds" sh "$HERE/httpmix.sh" "$H" "$U" "$P" 20
step "2 SSE clients 40 s with the request mix alongside" sh -c "sh '$HERE/ssecurl.sh' '$H' '$U' '$P' 40 '$HERE/sse1-$LABEL.txt' & sh '$HERE/ssecurl.sh' '$H' '$U' '$P' 40 '$HERE/sse2-$LABEL.txt' & sh '$HERE/httpmix.sh' '$H' '$U' '$P' 12; wait"
step "idle 15 s after" sleep 15
