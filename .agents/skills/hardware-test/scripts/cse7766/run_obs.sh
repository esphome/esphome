#!/bin/zsh
# usage: run_obs.sh IP SECONDS LABEL [OUTDIR] [NAME_FILTER]
# Observes states and logs over the API with a concurrent ping and prints a summary: the device-side
# "frames parsed" counter per 10 s, gaps over 5 s between voltage updates seen by the API, and warnings.
# Needs a python with aioesphomeapi (PYTHON, default python3).
D=${0:A:h}
IP=$1; SECS=$2; L=$3; S=${4:-.}; F=${5:-}
PY=${PYTHON:-python3}
ping -i 1 -c $SECS $IP > $S/ping-$L.txt 2>&1 &
P=$!
timeout $((SECS + 40)) $PY $D/observe.py $IP $SECS $S/obs-$L.txt "$F" 2>&1 | sed "s/$IP/<ip>/g"
wait $P
echo "ping: $(tail -2 $S/ping-$L.txt | head -1 | sed "s/$IP/<ip>/g") $(tail -1 $S/ping-$L.txt)"
python3 - "$S/obs-$L.txt" <<'PY'
import re, sys
txt = open(sys.argv[1]).read()
fr = [int(m.group(1)) for m in re.finditer(r"frames parsed: (\d+) in 10 s", txt)]
print(f"device frame counter per 10 s: n={len(fr)} min={min(fr) if fr else None} max={max(fr) if fr else None} mean={sum(fr)/len(fr) if fr else 0:.1f}")
ts = [float(m.group(1)) for m in re.finditer(r"^\[\s*([0-9.]+)\] .*Voltage: ", txt, re.M)]
gaps = [round(b - a, 1) for a, b in zip(ts, ts[1:]) if b - a > 5]
print(f"API voltage updates {len(ts)}, gaps over 5 s: {gaps}")
warn = [l for l in txt.splitlines() if "LOG" in l and ("[W]" in l or "[E]" in l or "Invalid" in l)]
print(f"warnings/errors in log: {len(warn)}", *warn[:3], sep="\n  ")
PY
