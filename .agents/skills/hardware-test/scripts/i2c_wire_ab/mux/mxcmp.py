"""mxcmp.py BASE PR: one capture each; per case rc and recorded stream hash must match; alloc summary."""

import pathlib
import re
import sys

MISSING = ("?", 0, "", "")


def load(path: str) -> dict[str, tuple]:
    cases = {}
    for line in (
        pathlib.Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
    ):
        if m := re.search(
            r"MX (.+?)\|rc=(-?\d+)\|alloc=(\d+)\|fnv=([0-9A-F]+)\|(.*)$", line
        ):
            cases[m.group(1)] = (
                m.group(2),
                int(m.group(3)),
                m.group(4),
                m.group(5).strip(),
            )
    return cases


b, p = load(sys.argv[1]), load(sys.argv[2])
print("cases", len(b), len(p))
diff = [k for k in b if k not in p or b[k][0] != p[k][0] or b[k][2] != p[k][2]]
print("rc/stream differences:", len(diff))
for k in diff[:10]:
    q = p.get(k, MISSING)
    print(
        "  ", k, "\n     base", b[k][0], b[k][3][:120], "\n     pr  ", q[0], q[3][:120]
    )
for name, d in (("base", b), ("pr", p)):
    wr = [k for k in d if k.startswith("wreg")]
    first = {
        s: next((x for x in range(301) if d.get(f"{s} len={x}", MISSING)[1]), None)
        for s in ("wreg8", "wreg16")
    }
    other = [k for k in d if not k.startswith("wreg") and d[k][1]]
    print(
        f"{name}: allocating register writes {sum(d[k][1] > 0 for k in wr)}/{len(wr)} first {first}; other {other}"
    )
