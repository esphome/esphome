"""Count SYNCED lines of drive.py's hwtest-disc-* clients logged after the device's disconnect line (a late fire).
Usage: order.py CAPTURE"""

import pathlib
import re
import sys

late = ok = 0
seen_disc = set()
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as f:
    for line in f:
        if m := re.search(r"\]: (hwtest-disc-\d+) \(.*\): disconnected", line):
            seen_disc.add(m.group(1))
        elif m := re.search(r"\]: SYNCED direct info=(hwtest-disc-\d+)", line):
            if m.group(1) in seen_disc:
                late += 1
                print("LATE", line.strip())
            else:
                ok += 1
print(
    f"synced before disconnect request answered: {ok}, synced after: {late}, "
    f"disconnect requests answered: {len(seen_disc)}"
)
