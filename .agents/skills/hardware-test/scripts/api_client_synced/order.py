"""Count SYNCED lines of drive.py's hwtest-disc-* clients logged after the device's disconnect line (a late fire).
Usage: order.py CAPTURE"""

import pathlib
import re
import sys

late = ok = discs = 0
seen_disc = set()
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as f:
    for line in f:
        if m := re.search(r"\]: (hwtest-disc-\d+) \(.*\): (dis)?connected", line):
            # names repeat across runs in one capture; a new connection clears the mark
            if m.group(2):
                seen_disc.add(m.group(1))
                discs += 1
            else:
                seen_disc.discard(m.group(1))
        elif m := re.search(r"\]: SYNCED direct info=(hwtest-disc-\d+)", line):
            if m.group(1) in seen_disc:
                late += 1
                print("LATE", line.strip())
            else:
                ok += 1
print(
    f"synced before disconnect request answered: {ok}, synced after: {late}, "
    f"disconnect requests answered: {discs}"
)
if not discs:  # zero SYNCED lines is a valid no-wait result
    sys.exit(f"no hwtest-disc-* disconnect lines in {sys.argv[1]}")
