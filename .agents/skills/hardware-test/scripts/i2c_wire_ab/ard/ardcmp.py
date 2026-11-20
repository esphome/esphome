"""ardcmp.py BASE_SER PR_SER [--reads-by-data]: per case rc and wire hash must match; alloc summary, rc counts and
wire shapes. --reads-by-data compares read cases by the returned data instead (BK72xx has no sniffer, so its wire
hash is that of an empty stream)."""

import argparse
from collections import Counter
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
from snlog import first_alloc, is_read, load  # noqa: E402

ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
ap.add_argument("base")
ap.add_argument("pr")
ap.add_argument("--reads-by-data", action="store_true")
args = ap.parse_args()
b, p = load(args.base, "SW"), load(args.pr, "SW")
print("cases", len(b), len(p))


def key(label: str, case: dict) -> tuple:
    if args.reads_by_data and is_read(label):
        return case["rc"], case["rd"]
    return case["rc"], case["fnv"]


diff = [k for k in b if k not in p or key(k, b[k]) != key(k, p[k])]
print("rc/wire (or read data) differences:", len(diff))
for k in diff[:20]:
    print("  ", k, "\n     base", b[k], "\n     pr  ", p.get(k))
for name, d in (("base", b), ("pr", p)):
    wr = [k for k in d if k.startswith("wreg")]
    allocs = {k: v["alloc"] for k, v in d.items()}
    first = {s: first_alloc(allocs, s) for s in ("wreg8", "wreg16")}
    print(
        f"{name}: write_register calls that allocated {sum(allocs[k] > 0 for k in wr)} of {len(wr)}; first "
        f"allocating len {first}; other allocs {[k for k in d if not k.startswith('wreg') and allocs[k]]}"
    )
    print(
        "   rc counts:",
        dict(Counter(v["rc"] for v in d.values())),
        "| wire shapes:",
        dict(
            Counter(
                re.sub(r"[0-9A-F]{2}", "xx", v["wire"]) for v in d.values()
            ).most_common(6)
        ),
    )
