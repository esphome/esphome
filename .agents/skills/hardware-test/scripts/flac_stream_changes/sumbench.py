"""sumbench.py HOST_FNV CAPTURE...: per clip real-time factor range and median, and whether every run's PCM hash
and frame count match the host decode (HOST_FNV lines: name length hash)."""

import pathlib
import statistics
import sys

from benchres import FILES, load

host = {
    ln.split()[0]: ln.split()[2]
    for ln in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
    if ln.strip()
}
for path in sys.argv[2:]:
    rows = load(path)
    print(path)
    for f in FILES:
        if not (ms := rows.get(f)):
            print(f"  {f}: none")
            continue
        rt = [float(m[5]) for m in ms]
        if f not in host:
            print(f"  {f}: no host hash")
            continue
        ok = all(m[6] == host[f] and m[7] == "0" for m in ms)
        print(
            f"  {f}: n={len(ms)} rtf {min(rt):.2f}-{max(rt):.2f} med {statistics.median(rt):.2f} pcm_match={ok} "
            f"dram {ms[-1][8]} iram {ms[-1][9]} ps {ms[-1][10]}"
        )
