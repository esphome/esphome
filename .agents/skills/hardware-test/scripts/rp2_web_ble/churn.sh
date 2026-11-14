#!/bin/zsh
# H=<device ip> churn.sh TAG SECS: 4 parallel loops of short /events connections (0.2 to 1.5 s) for SECS, logging the
# uptime each stream reports; counts reboots from the uptime going backwards, then reads the crash report over the API
# with crash_report/peek.py. PASS needs streams that got data (at least as many as got nothing), no reboot and no
# replayed crash record; exits 1 otherwise. env: H, TREE or PY, API_KEY (unset for a plaintext API)
D=${0:A:h}; T=$1; S=$2; H=${H:?set H to the device address}; PY=${PY:-${TREE:?set TREE or PY}/venv/bin/python}; cd $D
rm -f $T-churn*.txt
end=$(( $(date +%s) + S ))
for k in 1 2 3 4; do
  ( while (( $(date +%s) < end )); do m=0.$(( RANDOM % 9 + 2 )); [[ $((RANDOM % 3)) == 0 ]] && m=1.5
    r=$(curl -s -N --connect-timeout 1 -m $m http://$H/events 2>/dev/null | grep -a -o '"uptime":[0-9]*' | head -1)
    echo "$(date +%s) ${r:-none}"; done > $T-churn$k.txt ) &
done; wait
cat $T-churn*.txt | sort -n > $T-churn.txt
FAILS=0; fail() { echo "FAIL: $*"; FAILS=$((FAILS + 1)); }
read -r OK NONE REBOOTS < <(awk '$2 ~ /uptime/ {split($2,a,":"); b=$1-a[2]; if (pb!="" && b-pb>3) n++; pb=b; c++} $2=="none"{x++} END{print c+0, x+0, n+0}' $T-churn.txt)
echo "streams_with_ping=$OK none=$NONE reboots=$REBOOTS"
[ "$OK" -gt 0 ] && [ "$OK" -ge "$NONE" ] || fail "most streams got nothing (the stimulus did not reach the device)"
[ "$REBOOTS" = 0 ] || fail "$REBOOTS reboots"
PEEK=$($PY -I $D/../crash_report/peek.py $H) || fail "reading the crash report"
echo "crash report: $PEEK"
[[ $PEEK == *"replay lines 0"* ]] || fail "a crash record was replayed"
[ $FAILS = 0 ] || exit 1
echo PASS
