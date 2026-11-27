"""cmp.py PR_CAPTURE BASE_CAPTURE: per clip median and best real-time factor of two builds."""

import statistics
import sys

from benchres import FILES, load


def rtf(path: str) -> dict[str, list[float]]:
    """Real-time factors of the runs without a decode error."""
    out = {}
    for f, ms in load(path).items():
        if bad := sum(m[7] != "0" for m in ms):
            print(f"  {path} {f}: {bad} runs with err, left out")
        if ok := [float(m[5]) for m in ms if m[7] == "0"]:
            out[f] = ok
    return out


a, b = rtf(sys.argv[1]), rtf(sys.argv[2])
print(sys.argv[1], "vs", sys.argv[2])
for f in FILES:
    if f not in a or f not in b:
        print(f"  {f}: missing")
        continue
    ma, mb = statistics.median(a[f]), statistics.median(b[f])
    ba, bb = max(a[f]), max(b[f])
    print(
        f"  {f} n={len(a[f])}/{len(b[f])} median {ma:.1f} vs {mb:.1f} ({(ma / mb - 1) * 100:+.1f}%)  "
        f"best {ba:.1f} vs {bb:.1f} ({(ba / bb - 1) * 100:+.1f}%)"
    )
