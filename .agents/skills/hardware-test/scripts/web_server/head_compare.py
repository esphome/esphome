"""Compare two raw /events heads saved by stream_retest.py head: numbers and the device name masked.

usage: head_compare.py A.bin B.bin NAME_A NAME_B [CHUNKS]
Prints the first CHUNKS (default 6) masked chunks of each side and whether they are identical. Exits 1 when they
differ, or when a file has no HTTP head or no SSE event (an empty or truncated capture).
"""

import pathlib
import re
import sys

a, b, na, nb = sys.argv[1:5]
n = int(sys.argv[5]) if len(sys.argv) > 5 else 6


def chunks(path: str, name: str) -> list[str]:
    raw = pathlib.Path(path).read_bytes()
    head, _, body = raw.partition(b"\r\n\r\n")
    out = [head.decode(errors="replace")]
    while body and len(out) <= n:
        i = body.find(b"\r\n")
        if i < 0:
            break
        size = int(body[:i].split(b";")[0], 16)
        end = i + 2 + size
        # stop at the last chunk or one cut short by the capture, so a truncated file never counts as an event
        if size == 0 or body[end : end + 2] != b"\r\n":
            break
        out.append(body[i + 2 : i + 2 + size].decode(errors="replace"))
        body = body[i + 4 + size :]
    return [re.sub(r"\d+(\.\d+)?", "N", c.replace(name, "NAME")) for c in out]


ca, cb = chunks(a, na), chunks(b, nb)
for path, c in ((a, ca), (b, cb)):
    # the first chunk can be a bare blank line, so look for a real event line
    if not c[0].startswith("HTTP/") or not any("event:" in x for x in c[1:]):
        sys.exit(f"FAIL: {path} has no HTTP head or no SSE event")
for i, (x, y) in enumerate(zip(ca, cb, strict=False)):
    print(f"--- chunk {i} {'same' if x == y else 'DIFFERENT'}")
    print(repr(x)[:400])
    if x != y:
        print(repr(y)[:400])
print("IDENTICAL" if ca == cb else "DIFFER")
sys.exit(0 if ca == cb else 1)
