#!/bin/zsh
# sserun.sh HOST TAG SECS MODE(alone|api|pages) [N] : N sequential SSE streams (curl -m SECS each) with a pcap, a 1 s
# netstat poll and optional API log stream or page loads; after each stream ends, a fresh /events and GET / probe.
setopt null_glob
D=${0:A:h}; H=$1; T=$2; S=$3; M=$4; N=${5:-1}
PY=${PY:?set PY}; IFACE=${IFACE:-en0}  # the host interface that reaches the device
cd $D
rm -f $T-*.txt $T.pcap $T-api*.log
tcpdump -i $IFACE -n -s 200 -w $T.pcap host $H and port 80 2>$T-tcpdump.err & TP=$!
(while true; do echo "$(date +%s) $(netstat -anp tcp | grep -F "$H.80 " | tr -s ' ' | tr '\n' '|')"; sleep 1; done) > $T-netstat.txt 2>&1 & NP=$!
sleep 1
TOT=$(( (S+12)*N + 10 ))
[[ $M == api || $M == apipages ]] && { API_KEY=$(cat apikey.txt) $PY -I apilog.py $H $TOT $D/$T-api.log > $T-apisum.txt & AP=$!; sleep 3 }
[[ $M == pages || $M == apipages ]] && { (for i in $(seq 1 $((TOT/5))); do echo "$(date +%s) $(curl -s -o /dev/null -w '%{http_code} %{time_total}' -m 10 http://$H/)"; sleep 5; done) > $T-pages.txt & PP=$!; }
for i in $(seq 1 $N); do
  F=$T-sse$i.txt
  ( curl -s -N -m $S http://$H/events; echo "CURL_EXIT $?" ) | $PY -I ts.py > $F
  awk -v i=$i 'NR==1{t0=$1} /event: state/{c++; last=$1} /event: log/{l++} /CURL_EXIT/{e=$3; te=$1} END{printf "stream %d: states=%d last_state=+%.1fs logs=%d curl_exit=%s at +%.1fs\n", i, c, last-t0, l, e, te-t0}' $F | tee -a $T-summary.txt
  echo "  probe fresh_events(8s)=$(curl -s -N -m 8 http://$H/events | grep -c 'event: state') get_root=$(curl -s -o /dev/null -w '%{http_code}/%{size_download}B/%{time_total}s' -m 10 http://$H/)" | tee -a $T-summary.txt
done
[[ -n $AP ]] && wait $AP && cat $T-apisum.txt | tee -a $T-summary.txt
[[ -n $PP ]] && kill $PP 2>/dev/null
[[ $M == api || $M == apipages ]] && echo "api-after: $(API_KEY=$(cat apikey.txt) $PY -I apilog.py $H 8 $D/$T-apiafter.log)" | tee -a $T-summary.txt
kill $TP $NP 2>/dev/null; wait 2>/dev/null
