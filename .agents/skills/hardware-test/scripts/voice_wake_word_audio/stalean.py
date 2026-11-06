"""Per stale.py trial: clip length, where the wake phrase sits, and loud audio (the marker word) before it.

usage: stalean.py OUT   (stale.py's log; reads OUT.t<N>.pcm)   env: WAKE_REF (default wake16k.wav)
"""

import json
import pathlib
import sys

from audio_ref import env, find_phrase
import numpy as np

for line in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
    if "RESULT" not in line:
        continue
    m = json.loads(line.split("RESULT ", 1)[1])
    if not m["detected"]:
        print(f"t{m['t']} not detected")
        continue
    x = np.frombuffer(
        pathlib.Path(f"{sys.argv[1]}.t{m['t']}.pcm").read_bytes(), dtype=np.int16
    ).astype(np.float32)
    clip = x[: m["ww_ms"] * 16]
    k, corr, _ = find_phrase(x)
    pre = env(clip)[: max(0, k - 5)]
    loud = int((pre > 1500).sum()) * 10
    print(
        f"t{m['t']} clip {m['ww_ms']} ms, phrase at {k * 10} ms (corr {corr:.2f}), loud audio before phrase {loud} ms"
    )
