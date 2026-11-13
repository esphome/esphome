#!/bin/zsh
# H=<device ip> churn.sh TAG SECS: 4 parallel loops of short /events connections (0.2 to 1.5 s) for SECS, logging the
# uptime each stream reports; counts reboots from the uptime going backwards, then reads the crash report over the API
# with crash_report/peek.py. env: H, TREE or PY, API_KEY (unset for a plaintext API)
D=${0:A:h}; T=$1; S=$2; H=${H:?set H to the device address}; PY=${PY:-${TREE:?set TREE or PY}/venv/bin/python}; cd $D
rm -f $T-churn*.txt
end=$(( $(date +%s) + S ))
for k in 1 2 3 4; do
  ( while (( $(date +%s) < end )); do m=0.$(( RANDOM % 9 + 2 )); [[ $((RANDOM % 3)) == 0 ]] && m=1.5
    r=$(curl -s -N --connect-timeout 1 -m $m http://$H/events 2>/dev/null | grep -a -o '"uptime":[0-9]*' | head -1)
    echo "$(date +%s) ${r:-none}"; done > $T-churn$k.txt ) &
done; wait
cat $T-churn*.txt | sort -n > $T-churn.txt
awk '$2 ~ /uptime/ {split($2,a,":"); b=$1-a[2]; if (pb!="" && b-pb>3) n++; pb=b; c++} $2=="none"{x++} END{printf "streams_with_ping=%d none=%d reboots=%d ", c, x, n+0}' $T-churn.txt
echo "crash report: $($PY -I $D/../crash_report/peek.py $H)"
