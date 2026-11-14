#!/bin/zsh
# flow.sh NAME CONFIG HOST [SERIAL_PORT]: the RP2 scheduler lock flow (#22688). Uploads CONFIG (picow.yaml or evb.yaml)
# as NAME to DEVICE (default HOST, an OTA), captures SERIAL_PORT for the whole run when given, polls the Fast counter,
# then runs the removed item race, IRQ defer and IRQ cancel actions, connstress 500x4 and 500x2, 90 s of REST churn,
# REST churn and connstress together, and the IRQ actions again. PASS needs every action to pass, connstress 500x2
# without a failure, at least half of each REST churn answered 200, at least 90% of the counter polls answered, no
# reboot (counter reset) and, with SERIAL_PORT, no crash or assert line and a capture that lasted the run; exits 1
# otherwise. connstress at 4 is diagnostic: the API has 3 sockets on these builds, so 4 at a time always shows some
# "dropped after hello" failures.
# env: TREE, PY, DEVICE, API_KEY (unset for a plaintext API)
D=${0:A:h}; N=$1; C=$2; H=$3; SP=$4; S=$D/..; cd $D
T=${TREE:?set TREE}; PY=${PY:-$T/venv/bin/python}; L=$D/$N-flow.txt; : > $L
FAILS=0; fail() { echo "FAIL: $*" | tee -a $L; FAILS=$((FAILS + 1)); }
log() { echo "$*" | tee -a $L; }
# restchurn.sh prints "REST 200:n 404:m ..."; fail when fewer than half of the POSTs answered 200.
restok() {
  local ok all
  read ok all < <(echo "$2" | tr ' ' '\n' | awk -F: 'NF == 2 {a += $2; if ($1 == 200) o = $2} END {print o+0, a+0}')
  [ "$all" -gt 0 ] && [ $((ok * 2)) -ge "$all" ] || fail "$1: $ok of $all POSTs answered 200"; }
heap() { curl -s -m 4 "http://$H/sensor/Heap%20free" | grep -o '"value":[0-9]*' | cut -d: -f2; }
up() { for _ in $(seq 1 60); do nc -z -G 1 $H 6053 2>/dev/null && return 0; sleep 2; done; return 1; }
$S/pio_build.sh upload $N $D/$C ${DEVICE:-$H} || exit 1
up || { echo "FAIL: API port never opened"; exit 1; }; sleep 8
if [ -n "$SP" ]; then $PY $S/serial_capture.py $SP 3600 $D/$N-serial.log 2> $D/$N-cap.err & CAP=$!; sleep 3; fi
( while true; do echo "$(date +%s) $(curl -s -m 3 "http://$H/sensor/Fast%20counter" | grep -o '"value":[0-9]*')"; sleep 2; done ) > $D/$N-ctr.txt 2>&1 &
CTR=$!; trap 'kill $CTR $CAP 2>/dev/null' EXIT
actions() {
  for a in "run_test:Test Complete:20" "run_defer:DEFER RESULT:30" "run_cancel:CANCEL RESULT:30"; do
    IFS=: read s pat t <<< "$a"; [ "$1" = again ] && [ $s = run_test ] && continue
    out=$($PY -I $D/svcrun.py $H $s "$pat" $t 2>&1) || fail "$s ($1)"; log "$out"; sleep 2
  done
}
log "heap at start: $(heap)"
actions first
$PY -I $S/api/connstress.py $H 500 4 > $D/$N-cs4.txt 2>&1; log "connstress 500x4: $(grep -a -E '^Success|^RESULT' $D/$N-cs4.txt | tr '\n' ' ')"
sleep 5
$PY -I $S/api/connstress.py $H 500 2 > $D/$N-cs2.txt 2>&1 || fail "connstress 500x2"
log "connstress 500x2: $(grep -a -E '^Success|^RESULT' $D/$N-cs2.txt | tr '\n' ' ')"
sleep 5; R=$($D/restchurn.sh $H 90 $D/$N-rest.txt); log "REST churn 90 s: $R"; restok "REST churn 90 s" "$R"
sleep 5; log "heap after REST churn: $(heap)"
$D/restchurn.sh $H 120 $D/$N-rest2.txt > $D/$N-rest2.sum & RP=$!
$PY -I $S/api/connstress.py $H 500 4 > $D/$N-csrest.txt 2>&1
log "connstress 500x4 during REST churn: $(grep -a -E '^Success|^RESULT' $D/$N-csrest.txt | tr '\n' ' ')"
wait $RP; R=$(cat $D/$N-rest2.sum); log "REST during connstress: $R"; restok "REST during connstress" "$R"
sleep 5; log "heap at end: $(heap)"
actions again
[ -n "$SP" ] && { kill -0 $CAP 2>/dev/null || fail "the capture ended before the run did (see $D/$N-cap.err)"; }
kill $CTR $CAP 2>/dev/null; wait $CAP 2>/dev/null; trap - EXIT
RESETS=$(grep -o 'value":[0-9]*' $D/$N-ctr.txt | cut -d: -f2 | awk 'NR>1 && $1<p {r++} {p=$1} END {print r+0}')
POLLS=$(wc -l < $D/$N-ctr.txt | tr -d ' '); BADPOLLS=$(grep -vc value $D/$N-ctr.txt)
log "counter resets (reboots): $RESETS, failed counter polls: $BADPOLLS of $POLLS"
[ "$RESETS" = 0 ] || fail "$RESETS reboots"
# A wedged web_server or a board that is down answers no polls, which would otherwise read as no reboot.
[ "$POLLS" -gt 0 ] && [ $((BADPOLLS * 10)) -le "$POLLS" ] || fail "$BADPOLLS of $POLLS counter polls failed"
if [ -n "$SP" ]; then
  CRASH=$(grep -a -c -i -E 'CRASH|panic|HardFault|assert' $D/$N-serial.log)
  log "serial: crash lines $CRASH, boots $(grep -a -c 'Log initialized' $D/$N-serial.log)"
  [ "$CRASH" = 0 ] || fail "$CRASH crash or assert lines"
fi
[ $FAILS = 0 ] || exit 1
log PASS
