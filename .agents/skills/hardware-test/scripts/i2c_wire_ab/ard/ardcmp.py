"""ardcmp.py BASE_SER PR_SER [--reads-by-data]: per case rc and wire hash (and read data) must match; alloc summary."""

from collections import Counter
import pathlib
import re
import sys


def load(p):
    d = {}
    with pathlib.Path(p).open(encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = re.search(
                r"SW (.+?)\|rc=(-?\d+)\|alloc=(\d+)\|ev=(\d+)\|amb=(\d+)\|ovf=(\d+)\|fnv=([0-9A-F]+)\|(.*?)\|rd=(.*)$",
                line,
            )
            if m:
                d[m.group(1)] = {
                    "rc": int(m.group(2)),
                    "alloc": int(m.group(3)),
                    "fnv": m.group(7),
                    "wire": m.group(8).strip(),
                    "rd": m.group(9).strip(),
                }
    return d


b, p = load(sys.argv[1]), load(sys.argv[2])
print("cases", len(b), len(p))
diff = [
    k for k in b if k not in p or b[k]["rc"] != p[k]["rc"] or b[k]["fnv"] != p[k]["fnv"]
]
print("rc/wire differences:", len(diff))
for k in diff[:20]:
    print("  ", k, "\n     base", b[k], "\n     pr  ", p.get(k))
for name, d in (("base", b), ("pr", p)):
    wr = [k for k in d if k.startswith("wreg")]
    print(
        f"{name}: write_register calls that allocated {sum(d[k]['alloc'] > 0 for k in wr)} of {len(wr)}; first "
        f"allocating len {{wreg8: {next((n for n in range(400) if d.get(f'wreg8 len={n}', {}).get('alloc')), None)}, "
        f"wreg16: {next((n for n in range(400) if d.get(f'wreg16 len={n}', {}).get('alloc')), None)}}}; "
        f"other allocs {[k for k in d if not k.startswith('wreg') and d[k]['alloc']]}"
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
