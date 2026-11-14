#!/bin/zsh
# churnflow.sh NAME CONFIG HOST: upload CONFIG as NAME to DEVICE (default HOST, an OTA), then ROUNDS (default 2) rounds
# of `churn.sh` for 90 s with the free heap before, right after and after a 30 s settle, and a main loop check (the
# Fast counter must advance). A board that fell into safe mode (API port closed) gets NAME again by OTA. PASS needs
# every churn.sh round to pass and the loop to keep running; exits 1 otherwise. env: TREE, PY, DEVICE, ROUNDS, API_KEY
D=${0:A:h}; N=$1; C=$2; H=$3; S=$D/..; cd $D
T=${TREE:?set TREE}; export PY=${PY:-$T/venv/bin/python}; FAILS=0
val() { curl -s -m 4 "http://$H/sensor/$1" | grep -o '"value":[0-9]*' | cut -d: -f2; }
loopchk() { local a b; a=$(val Fast%20counter); sleep 3; b=$(val Fast%20counter); [[ -n $a && -n $b && $b -gt $a ]]; }
ready() { for _ in $(seq 1 40); do [ -n "$(val Uptime)" ] && return 0; sleep 3; done; return 1; }
$S/pio_build.sh upload $N $D/$C ${DEVICE:-$H} || exit 1
ready || { echo "FAIL: $N never answered"; exit 1; }
echo "$N idle heap: $(val Heap%20free)"
for r in $(seq 1 ${ROUNDS:-2}); do
  sleep 10; hb=$(val Heap%20free)
  res=$(H=$H $D/churn.sh $N-churn$r 90 2>&1) || FAILS=$((FAILS + 1))
  sleep 5; ha=$(val Heap%20free); sleep 30; hs=$(val Heap%20free)
  loopchk && lp="loop ok" || { lp="LOOP STUCK"; FAILS=$((FAILS + 1)); }
  echo "$N round $r: $(echo $res | tr '\n' ' ') | heap before $hb, after $ha, settled $hs | $lp"
  if ! nc -z -G 2 $H 6053 2>/dev/null; then
    sleep 15
    nc -z -G 2 $H 6053 2>/dev/null || { echo "$N: API port closed after round $r (safe mode?), uploading again"
      $S/pio_build.sh upload $N $D/$C $H && ready || { echo "$N: recovery after round $r failed, stopping"
        FAILS=$((FAILS + 1)); break; }; }
  fi
done
[ $FAILS = 0 ] || { echo FAIL; exit 1; }
echo PASS
