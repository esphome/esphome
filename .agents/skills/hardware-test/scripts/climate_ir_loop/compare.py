"""compare.py A B [TOL] [--flat]: compare two runs (A-frames.json, B-frames.json from frames.py).

Per step (default): frames equal within TOL us and RX states equal.
--flat: the whole frame sequence in order, timing by timing, ignoring step boundaries (for the D1 mini, where a log
line can land in the next step); reports every item over TOL and the largest deviation elsewhere.
"""

import json
import pathlib
import sys

args = [a for a in sys.argv[1:] if a != "--flat"]
FLAT = "--flat" in sys.argv[1:]
TOL = int(args[2]) if len(args) > 2 else 0


def load(name):
    steps = json.loads(pathlib.Path(f"{name}-frames.json").read_text(encoding="utf-8"))
    # an empty run must not compare as equal to another empty run
    if not steps or not any(s["frames"] for s in steps):
        sys.exit(f"{name}-frames.json has no steps or no frames")
    return steps


def same_timing(u, v):
    return (u > 0) == (v > 0) and abs(u - v) <= TOL


def frame_dev(p, q):
    """Largest deviation of two frames, or None when they differ in length or a timing is over TOL."""
    if len(p) != len(q) or not all(
        same_timing(u, v) for u, v in zip(p, q, strict=False)
    ):
        return None
    return max((abs(u - v) for u, v in zip(p, q, strict=False)), default=0)


def flat(a, b):
    fa = [f for s in a for f in s["frames"]]
    fb = [f for s in b for f in s["frames"]]
    mx = bad = items = 0
    print("frames", len(fa), len(fb))
    if len(fa) != len(fb):
        print("DIFF frame counts differ (a lost or extra transmission)")
        bad += 1
    for i, (p, q) in enumerate(zip(fa, fb, strict=False)):
        if len(p) != len(q):
            print("frame", i, "length", len(p), len(q))
            bad += 1
            continue
        for j, (u, v) in enumerate(zip(p, q, strict=False)):
            items += 1
            if same_timing(u, v):
                mx = max(mx, abs(u - v))
            else:
                print("frame", i, "item", j, u, v)
                bad += 1
    print(f"items {items} over_tol {bad} max_dev_within_tol {mx}")
    return bad == 0


def per_step(a, b):
    if len(a) != len(b):
        print(f"DIFF step counts {len(a)} vs {len(b)} (an interrupted run?)")
        sys.exit(1)
    same_f = same_s = maxdev = 0
    for x, y in zip(a, b, strict=False):
        assert x["step"] == y["step"]
        fa, fb = x["frames"], y["frames"]
        devs = [frame_dev(p, q) for p, q in zip(fa, fb, strict=False)]
        ok = len(fa) == len(fb) and None not in devs
        if ok:
            maxdev = max([maxdev, *devs])
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
            for k in x["rx"]:
                if x["rx"][k] != y["rx"].get(k):
                    print("   ", k, x["rx"][k], "|", y["rx"].get(k))
    print(
        f"steps {len(a)}/{len(b)} frames_equal {same_f} rx_states_equal {same_s} "
        f"total_frames {sum(len(s['frames']) for s in a)}/{sum(len(s['frames']) for s in b)} max_dev_us {maxdev}"
    )
    return same_f == len(a) and same_s == len(a)


if not (flat if FLAT else per_step)(load(args[0]), load(args[1])):
    sys.exit(1)
