"""nackcheck.py SER...: classify each DIFF case: 'chip NACK' when the wire equals the expected stream up to a byte the
chip NACKed (then STOP); anything else is listed."""

from collections import Counter
import pathlib
import re
import sys

for p in sys.argv[1:]:
    lines = pathlib.Path(p).read_text(encoding="utf-8", errors="replace").splitlines()
    c = Counter()
    other = []
    for i, line in enumerate(lines):
        m = re.search(r"SN (.+?)\|.*\|DIFF", line)
        if not m:
            continue
        got = lines[i + 1].split("SNGOT ")[1].split()
        exp = lines[i + 2].split("SNEXP ")[1].split()
        k = next(
            (j for j, t in enumerate(got) if t.endswith("-") and not t.startswith("r")),
            None,
        )
        ok = (
            k is not None
            and got[:k] == exp[:k]
            and got[k][:-1] == exp[k][:-1]
            and got[k + 1 :] == ["P"]
        )
        if ok:
            c[f"chip NACK at token {k}"] += 1
        else:
            other.append((m.group(1), " ".join(got)[:80], " ".join(exp)[:80]))
    print(p, dict(c), "other:", len(other))
    for o in other[:10]:
        print("   ", o)
