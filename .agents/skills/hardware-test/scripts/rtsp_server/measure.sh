#!/bin/zsh
# usage: measure.sh IP LABEL LOGFILE [OUTDIR]
# Runs the fixed RTSP measurement set against one build and appends a result line to OUTDIR/results.txt:
# ping RTT, two 12 s ffmpeg runs, two concurrent ffmpeg clients, and the raw receive rate with the default
# and a 1 MiB socket buffer. LOGFILE is the device capture, used to count "stalled" drops during the run.
# A measurement that produced nothing is written as ERR and the script exits 1, so a broken run never looks
# like a measured one. Needs ffmpeg and a python with the rtsp scripts' dependencies (PYTHON, default python3).
D=${0:A:h}
IP=$1; LABEL=$2; LOG=$3; S=${4:-.}
PY=${PYTHON:-python3}
[ -f "$LOG" ] || { echo "device capture not found: $LOG"; exit 1; }
rtt=$(ping -c 10 -i 0.2 $IP 2>&1 | tail -1 | awk -F/ '{print $5}')
stall0=$(grep -a -c "stalled" $LOG)
f1=$(timeout 40 ffmpeg -hide_banner -rtsp_transport tcp -i rtsp://$IP:554/ -t 12 -f null - 2>&1 | grep -o "frame= *[0-9]*" | tail -1 | tr -dc 0-9); sleep 2
f2=$(timeout 40 ffmpeg -hide_banner -rtsp_transport tcp -i rtsp://$IP:554/ -t 12 -f null - 2>&1 | grep -o "frame= *[0-9]*" | tail -1 | tr -dc 0-9); sleep 2
timeout 40 ffmpeg -hide_banner -rtsp_transport tcp -i rtsp://$IP:554/ -t 12 -f null - > $S/m-twoA.txt 2>&1 & A=$!; sleep 1
timeout 40 ffmpeg -hide_banner -rtsp_transport tcp -i rtsp://$IP:554/ -t 12 -f null - > $S/m-twoB.txt 2>&1 & B=$!
wait $A $B
fa=$(grep -o 'frame= *[0-9]*' $S/m-twoA.txt | tail -1 | tr -dc 0-9); fb=$(grep -o 'frame= *[0-9]*' $S/m-twoB.txt | tail -1 | tr -dc 0-9); sleep 2
raw=$(timeout 40 $PY $D/rawrate2.py $IP quiet 2>&1 | grep -o "[0-9.]* KiB/s" | head -1); sleep 2
raw2=$(timeout 40 $PY $D/rawrate2.py $IP bigbuf 2>&1 | grep -o "[0-9.]* KiB/s" | head -1); sleep 1
stall1=$(grep -a -c "stalled" $LOG)
bad=0
for v in rtt f1 f2 fa fb raw raw2; do
  if [ -z "${(P)v}" ]; then eval "$v=ERR"; bad=1; fi
done
line="$LABEL | rtt avg ${rtt} ms | ffmpeg single 12 s: ${f1}, ${f2} frames | two clients: ${fa} + ${fb} | python raw: ${raw} (1 MiB rcvbuf ${raw2}) | stalls logged: $((stall1 - stall0))"
echo "$line" | tee -a $S/results.txt
if [ $bad = 1 ]; then echo "one or more measurements failed (ERR above)"; exit 1; fi
