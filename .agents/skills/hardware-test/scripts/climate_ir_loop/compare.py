"""compare.py A B [TOL]: per step, frames equal (within TOL us) and RX states equal."""

import json
import pathlib
import sys

a = json.loads(pathlib.Path(f"{sys.argv[1]}-frames.json").read_text(encoding="utf-8"))
b = json.loads(pathlib.Path(f"{sys.argv[2]}-frames.json").read_text(encoding="utf-8"))
tol = int(sys.argv[3]) if len(sys.argv) > 3 else 0
same_f = same_s = 0
maxdev = 0
for x, y in zip(a, b, strict=False):
    assert x["step"] == y["step"]
    fa, fb = x["frames"], y["frames"]
    ok = len(fa) == len(fb) and all(
        len(p) == len(q)
        and all(
            (u > 0) == (v > 0) and abs(u - v) <= tol for u, v in zip(p, q, strict=False)
        )
        for p, q in zip(fa, fb, strict=False)
    )
    if ok:
        for p, q in zip(fa, fb, strict=False):
            for u, v in zip(p, q, strict=False):
                maxdev = max(maxdev, abs(u - v))
    same_f += ok
    st = x["rx"] == y["rx"]
    same_s += st
    if not ok or not st:
        print(
            "DIFF",
            x["step"],
            "frames",
            len(fa),
            len(fb),
            [len(p) for p in fa],
            [len(q) for q in fb],
        )
        if not st:
            for k in x["rx"]:
                if x["rx"][k] != y["rx"].get(k):
                    print("   ", k, x["rx"][k], "|", y["rx"].get(k))
print(
    f"steps {len(a)}/{len(b)} frames_equal {same_f} rx_states_equal {same_s} total_frames {sum(len(s['frames']) for s in a)}/{sum(len(s['frames']) for s in b)} max_dev_us {maxdev}"
)
