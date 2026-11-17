"""consist.py EXT: per frame, the trigger text (decode) must equal the dump chunks joined."""

import pathlib
import sys

lines = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
ok = bad = 0
i = 0
while i < len(lines):
    if lines[i].startswith("trig: TRIG len="):
        n = int(lines[i].split("=")[1])
        i += 1
        t = ""
        while i < len(lines) and lines[i].startswith("trig: TRIG "):
            t += lines[i][len("trig: TRIG ") :]
            i += 1
        while i < len(lines) and lines[i].startswith("remote.nec"):
            i += 1
        if i < len(lines) and lines[i] == "remote.pronto: Received Pronto: data=":
            i += 1
            d = ""
            while (
                i < len(lines)
                and lines[i].startswith("remote.pronto: ")
                and "Received" not in lines[i]
            ):
                d += lines[i][len("remote.pronto: ") :]
                i += 1
            if d == t and len(t) == n:
                ok += 1
            else:
                bad += 1
                print("MISMATCH", n, len(t), len(d))
        else:
            bad += 1
            print("MISSING dump after a trigger of", n)
        continue
    i += 1
print(f"{ok + bad} trigger frames, {ok} consistent, {bad} not")
