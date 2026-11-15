#!/bin/zsh
# looptime.sh NAME HOST SECS: the API connected (logs and states) plus a page load every 2 s for SECS, then a summary of
# the 16 ms interval gap lines from stress.yaml (average, worst) and the 50 ms re-arms per 5 s. Diagnostic only;
# compare two builds. Exits 1 when apilog.py fails, fewer than 2 windows were logged or most page loads did not
# return 200, so a broken or unloaded run is not read as a comparison.
# env: TREE or PY, API_KEY (unset for a plaintext API)
D=${0:A:h}; N=$1; H=$2; S=$3; PY=${PY:-${TREE:?set TREE or PY}/venv/bin/python}; cd $D
# apilog.py appends, so start from an empty log or an earlier run's windows count for this one
: > $D/$N-lt-api.log
$PY -I ../rp2_web_ble/apilog.py $H $S $D/$N-lt-api.log > /dev/null & AP=$!
( end=$(( $(date +%s) + S ))
  while (( $(date +%s) < end )); do curl -s -o /dev/null -m 5 -w '%{http_code}\n' http://$H/; sleep 2; done
) > $N-lt-web.txt & PP=$!
wait $AP; RC=$?; kill $PP 2>/dev/null
[ $RC = 0 ] || { echo "$N: apilog.py failed ($RC), see $N-lt-api.log"; exit 1; }
WOK=$(grep -c '^200$' $N-lt-web.txt); WALL=$(wc -l < $N-lt-web.txt | tr -d ' ')
[ "$WALL" -gt 0 ] && [ $((WOK * 2)) -ge "$WALL" ] ||
  { echo "$N: NOT LOADED, $WOK of $WALL page loads returned 200"; exit 1; }
grep -a -o "n=[0-9]* avg_gap_us=[0-9]* max_gap_us=[0-9]* btn=[0-9]* rearms=[0-9]*" $N-lt-api.log | tail -n +2 > $N-lt.txt
awk -v n=$N '{split($2,b,"="); split($3,c,"="); split($5,d,"="); k++; sa+=b[2]; if (c[2]>mx) mx=c[2]; sm+=c[2]; if (k>1) rr+=d[2]-pr; pr=d[2]}
  END {if (k < 2) {print n ": fewer than 2 windows"; exit 1}
  printf "%s: %d windows of 5 s, 16 ms interval avg gap %.0f us, mean window max %.0f us, worst %d us, re-arms per 5 s %.1f\n", n, k, sa/k, sm/k, mx, rr/(k-1)}' $N-lt.txt
