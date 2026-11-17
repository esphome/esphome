#!/bin/bash
# usage: soak.sh TAG HOST SECONDS [ha.py options]  serial capture plus HA stand-in, both bounded
# env: PORT (serial port), PY, API_KEY (falls back to apikey.txt next to the script; unset or - for plaintext)
W=$(cd "$(dirname "$0")" && pwd); cd "$W" || exit 1; T=$1; H=$2; S=$3; shift 3
PY=${PY:-python3}
PORT=${PORT:?serial port}
[ -z "${API_KEY+x}" ] && [ -f apikey.txt ] && API_KEY=$(cat apikey.txt)
$PY "$(dirname "$W")/serial_capture.py" "$PORT" "$S" "$T.txt" > /dev/null 2>&1 &
API_KEY=$API_KEY PYTHONUNBUFFERED=1 $PY ha.py "$H" "$T-ha.txt" "$S" "$@" > "$T-ha.err" 2>&1 &
wait
echo "$T done"
