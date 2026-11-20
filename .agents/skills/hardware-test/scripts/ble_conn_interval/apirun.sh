#!/bin/bash
# apirun.sh NAME YAML TAG SCENARIO...: OTA flash a proxy that has no USB console (proxy-s3.yaml on PoE, say), log it
# over the API to cap-TAG.txt and ping it to ping-TAG.txt while suite.sh TAG runs the scenarios.
# env: TREE, HOST (the proxy's address), PY, PEERS, API_KEY (else apikey.txt), as for suite.sh
D=$(cd "$(dirname "$0")" && pwd); cd "$D" || exit 1
N=$1 Y=$2 TAG=$3; shift 3
H=${HOST:?set HOST}; PY=${PY:-python3}
[ -z "${API_KEY+x}" ] && [ -f apikey.txt ] && API_KEY=$(cat apikey.txt)
export API_KEY
"$D/../pio_build.sh" upload "$N" "$Y" "$H" || exit 1
sleep 30
$PY apilogcap.py "$H" 5400 "cap-$TAG.txt" > /dev/null 2> "cap-$TAG.err" &
LOGCAP=$!
$PY pinger.py "$H" 5400 "ping-$TAG.txt" > /dev/null 2> "ping-$TAG.err" &
PING=$!
sleep 10
if ! kill -0 $LOGCAP 2>/dev/null || [ ! -s "cap-$TAG.txt" ]; then
  echo "log capture not running or empty, see cap-$TAG.err"; kill $LOGCAP $PING 2>/dev/null; exit 1
fi
echo "$(date +%T) $TAG flashed"
./suite.sh "$TAG" "$@"
kill $LOGCAP $PING 2>/dev/null
