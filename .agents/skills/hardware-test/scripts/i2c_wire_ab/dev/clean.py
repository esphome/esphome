"""clean.py [--nacks] NAME...: per build, cases with at least one capture (NAME-ser.txt, -ser2, -ser3) that is exactly
the expected stream or the expected stream cut by the chip's NACK; also every rc seen per case.
--nacks: per capture, count the DIFF cases that are a chip NACK and list the others."""

import argparse
from collections import Counter, defaultdict
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
from snlog import CAPTURES, load, nack_cut, runs  # noqa: E402


def clean(case: dict) -> bool:
    if case["verdict"] == "OK":
        return True
    return nack_cut(case.get("got", []), case.get("exp", [])) is not None


def nacks(name: str) -> None:
    for s in CAPTURES:
        path = pathlib.Path(f"{name}-ser{s}.txt")
        if not path.exists():
            continue
        c: Counter[str] = Counter()
        other = []
        for label, case in load(path).items():
            if case["verdict"] == "OK":
                continue
            got, exp = case.get("got", []), case.get("exp", [])
            if (j := nack_cut(got, exp)) is not None:
                c[f"chip NACK at token {j}"] += 1
            else:
                other.append((label, " ".join(got)[:80], " ".join(exp)[:80]))
        print(path, dict(c), "other:", len(other))
        for o in other[:10]:
            print("   ", o)


ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
ap.add_argument(
    "--nacks", action="store_true", help="classify the DIFF cases of each capture"
)
ap.add_argument("names", nargs="+")
args = ap.parse_args()
if args.nacks:
    for name in args.names:
        nacks(name)
    sys.exit()
allrc = defaultdict(set)
for name in args.names:
    caps = runs(name)
    bad = [k for k in caps[0] if not any(k in r and clean(r[k]) for r in caps)]
    for r in caps:
        for k, case in r.items():
            allrc[k].add(case["rc"])
    print(
        name,
        "captures",
        len(caps),
        "cases",
        len(caps[0]),
        "with no clean capture:",
        bad,
    )
print(
    "cases whose rc differs between any captures/builds:",
    [k for k, s in allrc.items() if len(s) > 1],
)
