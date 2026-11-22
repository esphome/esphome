#!/bin/bash
# suite.sh TAG HOST PORT|api [steps...]: run the drive.py scenarios against the proxy at HOST, with the device log
# from serial PORT (or over the API) and a pinger for the whole run. Results in TAG-<step>.jsonl, stderr in
# TAG-<step>.err, log in cap-TAG.txt, pings in ping-TAG.txt, step marks in marks-TAG.txt; then report.py TAG.
# env: PY, API_KEY (else apikey.txt), PEER_L, PEER_M, HK_NAME, PEER_ADDRS (peers.py), NSEC (notify seconds, 600),
#      R (disc and retry rounds, 15), TL (thirdL rounds, 8)
T=$1 H=$2 P=$3; shift 3
D=$(cd "$(dirname "$0")" && pwd); cd "$D" || exit 1
[ -z "${API_KEY+x}" ] && [ -f apikey.txt ] && API_KEY=$(cat apikey.txt)
export API_KEY PYTHONUNBUFFERED=1
PY=${PY:-python3}
STEPS=${*:-adv discL cacheL discM cacheM retryL retryM thirdeve thirdpd thirdL notify churn eve}
NSEC=${NSEC:-600}
if [ "$P" = api ]; then
  ($PY ../ble_conn_interval/apilogcap.py "$H" 30000 "cap-$T.txt" > /dev/null 2> "cap-$T.err" & echo $! > "cap-$T.pid")
else
  ($PY ../serial_capture.py "$P" 30000 "cap-$T.txt" --ms > /dev/null 2>&1 & echo $! > "cap-$T.pid"); fi
($PY ../ble_conn_interval/pinger.py "$H" 30000 "ping-$T.txt" > /dev/null 2>&1 & echo $! > "ping-$T.pid")
sleep 3
mark() { echo "$(date +%T) $*" | tee -a "marks-$T.txt"; }
# run STEP ROUNDS CMD ARGS...: an ARG of @N is replaced by the rounds still missing; reruns after an API drop
run() {
  local s=$1 want=$2; shift 2; local out=$T-$s.jsonl
  mark "start $s"
  for chunk in 1 2 3 4; do
    local have; have=$(grep -c '"round"' "$out" 2>/dev/null); have=${have:-0}
    [ "$have" -ge "$want" ] && break
    local args=("${@//@N/$((want - have))}")
    timeout 3600 $PY drive.py "$H" "${args[0]}" "$out" "${args[@]:1}" >> "$T-$s.err" 2>&1; local rc=$?
    have=$(grep -c '"round"' "$out" 2>/dev/null); have=${have:-0}
    mark "$s chunk $chunk rc=$rc rounds=$have"
    [ "$have" -ge "$want" ] || sleep 10
  done
  mark "end $s"
}
once() {
  local s=$1; shift; local out=$T-$s.jsonl
  mark "start $s"
  for chunk in 1 2; do
    timeout 3600 $PY drive.py "$H" "$1" "$out" "${@:2}" >> "$T-$s.err" 2>&1; local rc=$?
    mark "$s chunk $chunk rc=$rc"; [ $rc = 0 ] && break; sleep 10
  done
  mark "end $s"
}
for s in $STEPS; do
  case $s in
    adv) once adv adv 120 ;;
    discL) run discL "${R:-15}" disc L @N free ;;
    discM) run discM "${R:-15}" disc M @N free ;;
    cacheL) run cacheL 10 disc L @N cached ;;
    cacheM) run cacheM 10 disc M @N cached ;;
    retryL) run retryL "${R:-15}" disc L @N retry ;;
    retryM) run retryM "${R:-15}" disc M @N retry ;;
    thirdeve) run thirdeve 10 third @N eve ;;
    thirdpd) run thirdpd 8 third @N pd ;;
    thirdL) run thirdL "${TL:-8}" third @N L ;;
    notify) once notify notify "$NSEC" ;;
    churn) once churn churn 50 ;;
    eve) run eve 10 eve @N ;;
    *) mark "unknown step $s" ;;
  esac
done
kill "$(cat "ping-$T.pid")" "$(cat "cap-$T.pid")" 2>/dev/null
mark "suite done"
