"""summ.py DIR: per build medians (range) of the RESULT lines in DIR/results.txt, plus SDIO overflows, allocation
failures, error lines and the internal heap after boot and at its low point from DIR/cap-<build>-r*.txt."""

from pathlib import Path
import re
import statistics
import sys

H = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
KEYS = {"rx": "KBps", "tx": "KBps", "tcpburst": "median", "udpburst": "median"}
rows: dict[str, dict[str, list[float]]] = {}
failed: dict[str, int] = {}
for line in (H / "results.txt").read_text(encoding="utf-8").splitlines():
    m = re.match(r"(\S+) r(\d+) \S+ (\w+) (.*)", line)
    if not m or m[3] not in KEYS:
        continue
    d = rows.setdefault(m[1], {k: [] for k in KEYS})
    if v := re.search(r"RESULT .*" + KEYS[m[3]] + r"=(-?[\d.]+)", m[4]):
        d[m[3]].append(float(v[1]))
    else:  # a run that timed out or crashed, left out of the medians
        failed[m[1]] = failed.get(m[1], 0) + 1


def fmt(v: list[float]) -> str:
    return (
        f"{statistics.median(v):.0f} ({min(v):.0f} to {max(v):.0f}), n={len(v)}"
        if v
        else "-"
    )


for name, d in rows.items():
    texts = [
        p.read_text(encoding="utf-8", errors="replace")
        for p in sorted(H.glob(f"cap-{name}-r*.txt"))
    ]
    txt = "".join(texts)
    boot, low = [], []
    for t in texts:
        if hs := re.findall(r"heap free=\d+ internal=(\d+) min_internal=(\d+)", t):
            boot.append(int(hs[0][0]) // 1000)
            low.append(min(int(x[1]) for x in hs) // 1000)
    print(
        f"{name}: "
        + " | ".join(f"{k} {fmt(v)}" for k, v in d.items())
        + f" | failed runs {failed.get(name, 0)}"
        + f" | overflow {len(re.findall('overflow', txt, re.IGNORECASE))} oom {len(re.findall('mempool OOM', txt))}"
        f" alloc_fail {len(re.findall('alloc failed', txt))} E {len(re.findall(r'\[E\]', txt))}"
        f" | internal KB boot {min(boot, default='-')} to {max(boot, default='-')}"
        f" low {min(low, default='-')} to {max(low, default='-')}"
    )
