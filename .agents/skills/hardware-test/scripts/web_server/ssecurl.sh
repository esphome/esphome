#!/bin/sh
# ssecurl.sh HOST USER PASS SECONDS OUT : one authenticated SSE client through curl; counts state events and checks JSON
H=$1; U=$2; P=$3; T=$4; OUT=$5
timeout "$T" curl -sS -f -N -u "$U:$P" --anyauth "http://$H/events" > "$OUT"; rc=$?
[ "$rc" = 124 ] || { echo "sse: curl ended with status $rc before the $T s deadline"; exit 1; }
python3 - "$OUT" <<'PY'
import json, sys
states = invalid = 0; biggest = 0; ev = None
for line in open(sys.argv[1], "rb"):
    line = line.rstrip(b"\r\n")
    if line.startswith(b"event:"):
        ev = line[6:].strip()
    elif line.startswith(b"data:") and ev == b"state":
        states += 1; d = line[5:].strip(); biggest = max(biggest, len(d))
        try: json.loads(d)
        except ValueError: invalid += 1
    elif line == b"":
        ev = None
print(f"sse: {states} state events, {invalid} invalid JSON, largest {biggest} B")
sys.exit(1 if invalid or not states else 0)
PY
