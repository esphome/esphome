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
frames() {  # OUTFILE RC: the frame count of a run that finished on its own, nothing for a failed or killed one
  [ "$2" = 0 ] && grep -o 'frame= *[0-9]*' $1 | tail -1 | tr -dc 0-9
}
timeout 40 ffmpeg -hide_banner -rtsp_transport tcp -i rtsp://$IP:554/ -t 12 -f null - > $S/m-f1.txt 2>&1; f1=$(frames $S/m-f1.txt $?); sleep 2
timeout 40 ffmpeg -hide_banner -rtsp_transport tcp -i rtsp://$IP:554/ -t 12 -f null - > $S/m-f2.txt 2>&1; f2=$(frames $S/m-f2.txt $?); sleep 2
timeout 40 ffmpeg -hide_banner -rtsp_transport tcp -i rtsp://$IP:554/ -t 12 -f null - > $S/m-twoA.txt 2>&1 & A=$!; sleep 1
timeout 40 ffmpeg -hide_banner -rtsp_transport tcp -i rtsp://$IP:554/ -t 12 -f null - > $S/m-twoB.txt 2>&1 & B=$!
wait $A; ra=$?; wait $B; rb=$?
fa=$(frames $S/m-twoA.txt $ra); fb=$(frames $S/m-twoB.txt $rb); sleep 2
timeout 40 $PY $D/rawrate2.py $IP quiet > $S/m-raw.txt 2>&1 && raw=$(grep -o "[0-9.]* KiB/s" $S/m-raw.txt | head -1); sleep 2
timeout 40 $PY $D/rawrate2.py $IP bigbuf > $S/m-raw2.txt 2>&1 && raw2=$(grep -o "[0-9.]* KiB/s" $S/m-raw2.txt | head -1); sleep 1
stall1=$(grep -a -c "stalled" $LOG)
bad=0
for v in rtt f1 f2 fa fb raw raw2; do
  if [ -z "${(P)v}" ]; then eval "$v=ERR"; bad=1; fi
done
line="$LABEL | rtt avg ${rtt} ms | ffmpeg single 12 s: ${f1}, ${f2} frames | two clients: ${fa} + ${fb} | python raw: ${raw} (1 MiB rcvbuf ${raw2}) | stalls logged: $((stall1 - stall0))"
echo "$line" | tee -a $S/results.txt
if [ $bad = 1 ]; then echo "one or more measurements failed (ERR above)"; exit 1; fi
