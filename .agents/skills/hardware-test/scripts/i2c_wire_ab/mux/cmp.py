"""cmp.py BASE PR: host harness output; per config and case, rc and upstream stream must match; alloc summary."""

import pathlib
import sys


def load(path: str) -> tuple[dict, dict]:
    cases, extra = {}, {}
    for line in pathlib.Path(path).read_text(encoding="utf-8").splitlines():
        parts = line.split("|")
        if len(parts) == 2:
            extra[parts[0]] = parts[1]
            continue
        cfg, case, rc, alloc, wire = parts
        cases[(cfg, case)] = (rc, int(alloc.split("=")[1]), wire.strip())
    return cases, extra


b, be = load(sys.argv[1])
p, pe = load(sys.argv[2])
if b.keys() != p.keys():
    sys.exit("the two runs have different cases")
diff = [k for k in b if b[k][0] != p[k][0] or b[k][2] != p[k][2]]
print("cases", len(b), "| stream/rc differences:", len(diff))
for k in diff[:10]:
    print(
        "  ",
        k,
        "\n     base",
        b[k][0],
        b[k][2][:150],
        "\n     pr  ",
        p[k][0],
        p[k][2][:150],
    )
for cfg in sorted({k[0] for k in b}):
    for name, d, ex in (("base", b, be), ("pr", p, pe)):
        wr = [k for k in d if k[0] == cfg and k[1].startswith("wreg")]
        first = {
            s: next((n for n in range(301) if d[(cfg, f"{s} len={n}")][1]), None)
            for s in ("wreg8", "wreg16")
        }
        other = [
            k[1] for k in d if k[0] == cfg and not k[1].startswith("wreg") and d[k][1]
        ]
        print(
            f"  {cfg:16s} {name:4s} allocating register writes {sum(d[k][1] > 0 for k in wr)}/{len(wr)}"
            f" first {first} other {other[:4]} | {ex.get(cfg)}"
        )
