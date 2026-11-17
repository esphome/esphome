#!/bin/bash
# usage: soak.sh TAG HOST SECONDS [ha.py options]  serial capture plus HA stand-in, both bounded
# env: PORT (serial port), PY; the API key is read from apikey.txt next to the script
W=$(cd "$(dirname "$0")" && pwd); cd "$W" || exit 1; T=$1; H=$2; S=$3; shift 3
PY=${PY:-python3}
PORT=${PORT:?serial port}
$PY "$(dirname "$W")/serial_capture.py" "$PORT" "$S" "$T.txt" > /dev/null 2>&1 &
sleep 8
API_KEY=$(cat apikey.txt) PYTHONUNBUFFERED=1 $PY ha.py $H $T-ha.txt $S "$@" > $T-ha.err 2>&1 &
wait
echo "$T done"
