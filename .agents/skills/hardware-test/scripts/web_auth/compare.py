"""Compare two authfuzz.py runs (dev vs PR). usage: compare.py DEV_PREFIX PR_PREFIX"""

import json
import pathlib
import sys

a = {
    r["case"]: r
    for r in json.loads(
        pathlib.Path(sys.argv[1] + "-cases.json").read_text(encoding="utf-8")
    )
}
b = {
    r["case"]: r
    for r in json.loads(
        pathlib.Path(sys.argv[2] + "-cases.json").read_text(encoding="utf-8")
    )
}
print(f"{'case':45} {'dev':>12} {'pr':>12}")
for k in list(dict.fromkeys(list(a) + list(b))):
    ra, rb = a.get(k, {}), b.get(k, {})
    fa = f"{ra.get('status')}/{'ok' if ra.get('pass') else 'FAIL'}" if ra else "-"
    fb = f"{rb.get('status')}/{'ok' if rb.get('pass') else 'FAIL'}" if rb else "-"
    mark = (
        "  <== differs"
        if (ra.get("status"), ra.get("pass")) != (rb.get("status"), rb.get("pass"))
        else ""
    )
    print(f"{k:45} {fa:>12} {fb:>12}{mark}")
na, nb = a.get("noauth GET /", {}), b.get("noauth GET /", {})
print("\n401 head dev:\n" + na.get("head", "") + "\nbody:", repr(na.get("body_head")))
print("\n401 head pr:\n" + nb.get("head", "") + "\nbody:", repr(nb.get("body_head")))
