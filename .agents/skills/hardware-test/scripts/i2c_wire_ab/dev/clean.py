"""clean.py NAME...: per build, cases with at least one capture that is exactly the expected stream or the expected
stream cut by the chip's NACK; also every rc seen per case."""

from collections import defaultdict
import pathlib
import re
import sys


def verdicts(p):
    lines = pathlib.Path(p).read_text(encoding="utf-8", errors="replace").splitlines()
    out = {}
    for i, line in enumerate(lines):
        m = re.search(r"SN (.+?)\|rc=(-?\d+)\|.*\|(OK|DIFF)$", line)
        if not m:
            continue
        k, rc, v = m.group(1), m.group(2), m.group(3)
        if v == "OK":
            out[k] = (rc, True)
            continue
        got = lines[i + 1].split("SNGOT ")[1].split()
        exp = lines[i + 2].split("SNEXP ")[1].split()
        j = next(
            (j for j, t in enumerate(got) if t.endswith("-") and not t.startswith("r")),
            None,
        )
        out[k] = (
            rc,
            j is not None
            and got[:j] == exp[:j]
            and got[j][:-1] == exp[j][:-1]
            and got[j + 1 :] == ["P"],
        )
    return out


allrc = defaultdict(set)
for name in sys.argv[1:]:
    runs = [verdicts(f"{name}-ser{s}.txt") for s in ("", "2", "3")]
    bad = [k for k in runs[0] if not any(r[k][1] for r in runs)]
    for r in runs:
        for k, (rc, _) in r.items():
            allrc[k].add(rc)
    print(name, "cases", len(runs[0]), "with no clean capture:", bad)
print(
    "cases whose rc differs between any captures/builds:",
    [k for k, s in allrc.items() if len(s) > 1],
)
