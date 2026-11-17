"""flatcmp.py A B TOL: compare the whole raw frame sequence; report items over TOL and the largest deviation elsewhere."""

import json
import pathlib
import sys

fa = [
    f
    for s in json.loads(
        pathlib.Path(f"{sys.argv[1]}-frames.json").read_text(encoding="utf-8")
    )
    for f in s["frames"]
]
fb = [
    f
    for s in json.loads(
        pathlib.Path(f"{sys.argv[2]}-frames.json").read_text(encoding="utf-8")
    )
    for f in s["frames"]
]
tol = int(sys.argv[3])
mx = 0
bad = 0
items = 0
print("frames", len(fa), len(fb))
for i, (p, q) in enumerate(zip(fa, fb, strict=False)):
    if len(p) != len(q):
        print("frame", i, "length", len(p), len(q))
        bad += 1
        continue
    for j, (u, v) in enumerate(zip(p, q, strict=False)):
        items += 1
        if (u > 0) != (v > 0) or abs(u - v) > tol:
            print("frame", i, "item", j, u, v)
            bad += 1
        else:
            mx = max(mx, abs(u - v))
print(f"items {items} over_tol {bad} max_dev_within_tol {mx}")
