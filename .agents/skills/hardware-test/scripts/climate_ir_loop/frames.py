"""frames.py NAME: per step, the raw frames and the RX states. Prints JSON to NAME-frames.json."""

import json
import pathlib
import re
import sys

n = sys.argv[1]
steps = []
cur = None
with pathlib.Path(f"{n}-drive.txt").open(encoding="utf-8") as fh:
    for text in fh:
        m = re.match(r"(\d\d:\d\d:\d\d) STEP (.*)", text)
        if m:
            cur = {"t": m[1], "step": m[2], "rx": {}, "tx": {}, "frames": []}
            steps.append(cur)
        m = re.match(r"\s+after (\S+ \S+)\s+(.*)", text)
        if m and cur:
            (cur["rx"] if m[1].endswith("RX") else cur["tx"])[m[1]] = m[2]
frames = []
buf = None
with pathlib.Path(f"{n}-ser.txt").open(encoding="utf-8", errors="replace") as fh:
    for text in fh:
        m = re.match(r"\[(\d\d:\d\d:\d\d)\]\[I\]\[remote\.raw(?::\d+)?\]: ?(.*)", text)
        if not m:
            continue
        ts, body = m[1], m[2]
        if body.startswith("Received Raw:"):
            buf = [ts, body[len("Received Raw:") :]]
        elif buf is not None:
            buf[1] += body
        # the raw dumper splits long frames into lines that end with a comma; the last line does not
        if buf is not None and not body.rstrip().endswith(","):
            frames.append(
                (
                    buf[0],
                    [
                        int(x)
                        for x in buf[1].replace(" ", "").strip(",").split(",")
                        if x
                    ],
                )
            )
            buf = None
for ts, f in frames:
    owner = None
    for s in steps:
        if s["t"] <= ts:
            owner = s
    if owner:
        owner["frames"].append(f)
pathlib.Path(f"{n}-frames.json").write_text(json.dumps(steps), encoding="utf-8")
print(n, "steps", len(steps), "frames", sum(len(s["frames"]) for s in steps))
