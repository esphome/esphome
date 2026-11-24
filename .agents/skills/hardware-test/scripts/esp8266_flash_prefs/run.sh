#!/bin/bash
# run.sh TAG SECONDS HOST: serial capture to TAG-cap.log, `ping -i 0.05` for SECONDS (TAG-ping.txt; -t/-w keeps the
# summary, a ping killed by timeout loses it) and an API connection held with subscriptions (TAG-api.txt), all bounded.
# Then `crashes.py TAG-cap.log firmware.elf` and `crashes.py --trend TAG-cap.log` (scan-repro) or
# `israna.py TAG-cap.log` (isr-load).
# env: PORT, PY (needs aioesphomeapi and pyserial)
D=$(cd "$(dirname "$0")" && pwd); S=$(dirname "$D")
TAG=${1:?tag}; SECS=${2:?seconds}; H=${3:?host}; P=${PORT:?set PORT}; PY=${PY:-python3}
cd "$D" || exit 1
$PY "$S/serial_capture.py" "$P" "$SECS" "$TAG-cap.log" > /dev/null 2>&1 &
ping -i 0.05 -q "$([ "$(uname)" = Darwin ] && echo -t || echo -w)" "$SECS" "$H" > "$TAG-ping.txt" 2>&1 &
timeout $((SECS + 30)) $PY apihold.py "$H" "$SECS" > "$TAG-api.txt" 2>&1 &
wait
