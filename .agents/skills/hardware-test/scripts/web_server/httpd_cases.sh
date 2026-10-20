#!/bin/sh
# httpd_cases.sh HOST USER PASS LABEL CAPTURE : HTTPD task stack load for the web_server_idf task size; probe lines from
# CAPTURE, SSE client output into the working directory. A failed step ends the run with its status.
H=$1; U=$2; P=$3; LABEL=$4; LOG=$5; HERE=$(dirname "$0")
probes() { tail -n +$(($1+1)) "$LOG" | sed 's/\x1b\[[0-9;]*m//g' | grep -a -o "\[[0-9:]*\]\[I\]\[probe:[0-9]*\]: .*httpd min free stack [0-9]*\|\[[0-9:]*\]\[[WE]\]\[[a-z_.]*:[0-9]*\].*\|Guru.*" | grep -a -v "took a long time\|does not fit\|\]\[httpd\]: /" | sed 's/\[I\]\[probe:[0-9]*\]: loopTask min free stack \([0-9]*\), httpd min free stack \([0-9]*\)/ loop \1 httpd \2/' | uniq -f1 | cut -c1-140; }
step() { n0=$(wc -l < "$LOG"); echo "=== $LABEL: $1 ($(date +%T))"; shift; "$@" || { rc=$?; echo "step failed ($rc)"; exit $rc; }; echo "--- device log:"; probes "$n0"; }
step "idle 15 s" sleep 15
step "request mix, 20 rounds" sh "$HERE/httpmix.sh" "$H" "$U" "$P" 20
step "2 SSE clients 40 s with the request mix alongside" sh -c "sh '$HERE/ssecurl.sh' '$H' '$U' '$P' 40 '$PWD/sse1-$LABEL.txt' & p1=\$!; sh '$HERE/ssecurl.sh' '$H' '$U' '$P' 40 '$PWD/sse2-$LABEL.txt' & p2=\$!; sh '$HERE/httpmix.sh' '$H' '$U' '$P' 12 && wait \$p1 && wait \$p2"
step "idle 15 s after" sleep 15
