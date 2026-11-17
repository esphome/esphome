"""consist.py EXT: per frame, the trigger text (decode) must equal the dump chunks joined."""

import sys

from extract import read_lines, trigger_blocks

DUMP_START = "remote.pronto: Received Pronto: data="
DUMP = "remote.pronto: "

lines = read_lines(sys.argv[1])
ok = bad = 0
for n, t, i in trigger_blocks(lines):
    # dump: all prints NEC lines and the pronto binary sensor can log between the trigger and the Pronto dump
    while i < len(lines) and (
        lines[i].startswith("remote.nec") or "BS pronto" in lines[i]
    ):
        i += 1
    if i < len(lines) and lines[i] == DUMP_START:
        i += 1
        d = ""
        while (
            i < len(lines) and lines[i].startswith(DUMP) and "Received" not in lines[i]
        ):
            d += lines[i][len(DUMP) :]
            i += 1
        if d == t and len(t) == n:
            ok += 1
        else:
            bad += 1
            print("MISMATCH", n, len(t), len(d))
    else:
        bad += 1
        print("MISSING dump after a trigger of", n)
print(f"{ok + bad} trigger frames, {ok} consistent, {bad} not")
