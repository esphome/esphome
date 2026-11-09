"""Per stale.py trial: clip length, where the wake phrase sits, and loud audio (the marker word) before it.

usage: stalean.py OUT   (stale.py's log; reads OUT.t<N>.pcm)
env: LOUD_LEVEL (required, envelope level that counts as loud; calibrate it per setup from the printed peaks),
WAKE_REF (default wake16k.wav), MAX_LOUD_MS (default 100), MIN_CORR (default 0.5)

Exits 1 when LOUD_LEVEL is not set, no trial was found, or a trial was not detected, has no wake word audio, matches
the phrase weakly, has a phrase peak under twice LOUD_LEVEL (too quiet to see a marker) or has more than MAX_LOUD_MS of
loud audio before the phrase.
"""

import json
import os
import pathlib
import sys

from audio_ref import env, find_phrase
import numpy as np

LOUD_LEVEL = float(os.environ.get("LOUD_LEVEL") or 0)
MAX_LOUD_MS = int(os.environ.get("MAX_LOUD_MS", "100"))
MIN_CORR = float(os.environ.get("MIN_CORR", "0.5"))
failures: list[str] = []
trials = 0
for line in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
    if "RESULT" not in line:
        continue
    trials += 1
    m = json.loads(line.split("RESULT ", 1)[1])
    if not m["detected"]:
        print(f"t{m['t']} not detected")
        failures.append(f"t{m['t']} not detected")
        continue
    if not m.get("ww_ms"):
        print(f"t{m['t']} detected without wake word audio")
        failures.append(f"t{m['t']} no wake word audio")
        continue
    x = np.frombuffer(
        pathlib.Path(f"{sys.argv[1]}.t{m['t']}.pcm").read_bytes(), dtype=np.int16
    ).astype(np.float32)
    clip = x[: m["ww_ms"] * 16]
    k, corr, n = find_phrase(x)
    pre = env(clip)[: max(0, k - 5)]
    peak = float(env(x)[k : k + n].max(initial=0))
    pre_peak = float(pre.max(initial=0))
    loud = int((pre > LOUD_LEVEL).sum()) * 10 if LOUD_LEVEL else 0
    print(
        f"t{m['t']} clip {m['ww_ms']} ms, phrase at {k * 10} ms (corr {corr:.2f}), phrase peak {peak:.0f}, "
        f"peak before phrase {pre_peak:.0f}, loud audio before phrase "
        + (f"{loud} ms" if LOUD_LEVEL else "not judged")
    )
    if LOUD_LEVEL and peak < 2 * LOUD_LEVEL:
        failures.append(
            f"t{m['t']} phrase peak {peak:.0f} < 2 x LOUD_LEVEL, too quiet to judge"
        )
    if corr < MIN_CORR:
        failures.append(f"t{m['t']} phrase corr {corr:.2f} < {MIN_CORR}")
    if loud > MAX_LOUD_MS:
        failures.append(f"t{m['t']} {loud} ms loud audio before the phrase")
if not LOUD_LEVEL:
    failures.append("LOUD_LEVEL not set, calibrate it from the peaks above")
if not trials:
    failures.append("no RESULT lines")
if failures:
    print(f"FAIL: {failures}")
    sys.exit(1)
print("PASS")
