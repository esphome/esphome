#!/bin/sh
# sse_cases.sh HOST KEY LABEL CAPTURE : the SSE cases against templates/web-server-sse-dut.yaml with a console capture
# running into CAPTURE; KEY is the API key (- for plaintext). The burst client output goes to the working directory.
H=$1; K=$2; LABEL=$3; LOG=$4; PY=${PYTHON:-python3}; HERE=$(dirname "$0"); BTN=$HERE/../api_button.py
[ -r "$LOG" ] || { echo "usage: sse_cases.sh HOST KEY LABEL CAPTURE (CAPTURE must be a readable file)"; exit 2; }
show() { tail -n +$(($1+1)) "$LOG" | sed 's/\x1b\[[0-9;]*m//g' | grep -a '\[probe:\|\[W\]\|\[E\]\|web_server\|Guru\|abort\|Backtrace\|ESPHome version' | grep -a -v 'took a long time' | cut -c1-200; }
step() { n0=$(wc -l < "$LOG"); echo "=== $LABEL: $1 ($(date +%T))"; shift; "$@" || { rc=$?; echo "client failed ($rc), aborting"; exit $rc; }; echo "--- device log:"; show "$n0"; }
step "idle 15 s, no client" sleep 15
step "1 client 60 s" $PY "$HERE/sse.py" "$H" 60 --clients 1
step "4 clients 60 s" $PY "$HERE/sse.py" "$H" 60 --clients 4
step "1 reader plus a stalled client 45 s" $PY "$HERE/sse.py" "$H" 45 --clients 1 --stall
step "1 reader plus 10 abrupt closes 30 s" $PY "$HERE/sse.py" "$H" 30 --clients 1 --churn 10
n0=$(wc -l < "$LOG"); echo "=== $LABEL: 2 clients 25 s with the Burst button at 5 s ($(date +%T))"
$PY "$HERE/sse.py" "$H" 25 --clients 2 > "$PWD/burst-$LABEL.txt" 2>&1 &
pid=$!
sleep 5; $PY "$BTN" "$H" "$K" press "Burst" || { echo "button press failed"; exit 1; }
wait $pid || { cat "$PWD/burst-$LABEL.txt"; echo "SSE clients failed during the burst"; exit 1; }
cat "$PWD/burst-$LABEL.txt"; echo "--- device log:"; show "$n0"
step "idle 15 s after" sleep 15
