#!/bin/bash
# usage: bench.sh CONFIG DEVICE   build and upload CONFIG (bench-bk72xx or bench-esp32s3) to DEVICE (OTA address or
# serial port), then collect one benchmark pass over syslog. Run gen_streams.py in this directory first.
# Copy this directory into a scratch directory with a secrets.yaml (wifi_ssid, wifi_password, syslog_ip = this
# host's address) and run it there. env: TREE (esphome checkout under test), PY (checkout python), WAIT (seconds to
# wait for a pass, default 300)
D=$(cd "$(dirname "$0")" && pwd); C=${1:?config name}; DEV=${2:?device}; T=${TREE:?set TREE}
PY=${PY:-$T/venv/bin/python}; WAIT=${WAIT:-300}; OUT=$D/$C-syslog.txt
[ -f "$D/bench_streams.hpp" ] || { echo "FAIL: run gen_streams.py first"; exit 1; }
(cd "$T" && PYTHONUNBUFFERED=1 timeout 1500 $PY -m esphome run --no-logs "$D/$C.yaml" --device "$DEV" > "$D/$C-upload.log" 2>&1) \
  || { echo "FAIL: build or upload failed, see $C-upload.log"; exit 1; }
$PY - "$OUT" "$WAIT" << 'PYEOF'
import socket
import sys
import time

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("0.0.0.0", 5514))
s.settimeout(1)
end = time.time() + int(sys.argv[2])
with open(sys.argv[1], "w", encoding="utf-8") as out:
    while time.time() < end:
        try:
            d, _ = s.recvfrom(2048)
        except TimeoutError:
            continue
        line = d.decode(errors="replace").strip()
        if "bench" in line:
            print(line, file=out, flush=True)
            if "BENCHDONE" in line:
                break
PYEOF
grep -q BENCHDONE "$OUT" || { echo "FAIL: no complete pass within $WAIT s"; exit 1; }
grep -o 'bench:[0-9]*\]: .*' "$OUT"
BAD=$(grep 'best_us=' "$OUT" | grep -v 'res=1 ' | wc -l | tr -d ' ')
SHORT=$(grep -o 'out=[0-9]*/[0-9]*' "$OUT" | awk -F'[=/]' '$2 != $3' | wc -l | tr -d ' ')
[ "$BAD" = 0 ] && [ "$SHORT" = 0 ] && echo "PASS" || { echo "FAIL: $BAD cases did not finish, $SHORT short outputs"; exit 1; }
