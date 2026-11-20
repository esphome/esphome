"""Find where the played wake phrase sits in each saved channel of a va.py run.

usage: locate.py OUTDIR   env: WAKE_REF (16 kHz mono WAV of the phrase, default wake16k.wav)
"""

import json
import pathlib
import sys

from audio_ref import find_phrase, load
import numpy as np

for meta in sorted(
    pathlib.Path(sys.argv[1]).glob("run*.json"),
    key=lambda p: int(p.name[3:].split("-")[0]),
):
    m = json.loads(meta.read_text(encoding="utf-8"))
    out = [f"run {m['n']} {m['mode']}"]
    if ww := m["ww"]:
        out.append(
            f"id={ww['id']} p={ww['p']:.2f} cutoff={ww['cutoff']:.2f} ww={ww['bytes'] / 32:.0f}/{ww['bytes2'] / 32:.0f} ms"
        )
    for ch in (1, 2):
        x = load(meta.with_name(meta.stem + f"-ch{ch}.wav"))
        if len(x) < 16000:
            out.append(f"ch{ch} {len(x) / 16:.0f} ms")
            continue
        k, corr, n = find_phrase(x)
        out.append(
            f"ch{ch} {len(x) / 16:.0f} ms phrase {k * 10}-{(k + n) * 10} ms corr {corr:.2f} "
            f"rms {np.sqrt((x**2).mean()):.0f}"
        )
    print("  ".join(out))
