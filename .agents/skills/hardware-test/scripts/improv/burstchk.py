"""burstchk.py RAW [RAW...]: check SEQ lines in raw captures for gaps, duplicates and broken lines.

Counts per boot segment (split on 'Running through setup()'); a partial first or last line is ignored. Expects the
burst of `usj-heap-common.yaml`: `[burst]: SEQ <n>` plus `[burst]: plain line without arguments`. Exits 1 when a
segment has no SEQ lines, missing, duplicate or broken lines, or plain lines that do not pair with SEQ lines; the
part before the first setup line counts only if there is no other.
"""

import pathlib
import re
import sys

BURST = re.compile(
    rb"\[burst[^\]]*\]: (?:SEQ (\d+)|plain line without arguments)(\x1b\[0m)?\r?$"
)

bad = False
for path in sys.argv[1:]:
    data = pathlib.Path(path).read_bytes()
    segs = data.split(b"Running through setup()")
    print(f"== {path}: {len(segs) - 1} setup lines")
    for i, seg in enumerate(segs):
        lines = seg.split(b"\n")[1:-1]
        seqs, plain, broken = [], 0, 0
        for ln in lines:
            if b"[burst" not in ln and b"SEQ" not in ln and b"plain line" not in ln:
                continue
            # two records run together are a broken line
            m = ln.count(b"[burst") == 1 and BURST.search(ln)
            if m and m.group(1):
                seqs.append(int(m.group(1)))
            elif m:
                plain += 1
            else:
                broken += 1
                if broken <= 5:
                    print("  broken:", ln[:160])
        counted = i > 0 or len(segs) == 1
        if not seqs:
            bad |= counted
            print(f"  seg{i}: no SEQ lines, plain={plain}, broken={broken}")
            continue
        gaps = sum(b - a - 1 for a, b in zip(seqs, seqs[1:], strict=False) if b > a + 1)
        back = sum(1 for a, b in zip(seqs, seqs[1:], strict=False) if b <= a)
        # one SEQ and one plain line per tick; a pair cut at either end differs by 1
        lost_plain = abs(plain - len(seqs)) > 1
        bad |= counted and (gaps or back or broken or lost_plain) > 0
        print(
            f"  seg{i}: SEQ {seqs[0]}..{seqs[-1]} n={len(seqs)} missing={gaps} dup_or_back={back} plain={plain} broken={broken}"
        )
sys.exit(1 if bad else 0)
