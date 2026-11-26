"""Check every info=/addr= line of the trigger logs and pair each deferred read with its trigger run.
Usage: args.py CAPTURE ADDR [INFO_REGEX]
ADDR is the client address the device should report; INFO_REGEX matches a good client_info (default: a Home
Assistant version, aioesphomeapi or a hwtest-* client). Prints per kind counts and the first mismatches."""

import collections
import pathlib
import re
import sys

addr = sys.argv[2]
good_info = re.compile(
    sys.argv[3]
    if len(sys.argv) > 3
    else r"^(Home Assistant [\w.]+|aioesphomeapi|hwtest-[\w-]+)$"
)
ok = collections.Counter()
bad = collections.Counter()
pair_ok = collections.Counter()
pair_bad = collections.Counter()
shown = unpaired = 0
now = {}
line_re = re.compile(
    r"\]: (CONNECTED|DISCONNECTED|SYNCED) (direct|delayed1?|cap-now|cap-later|cap-copied)"
    r"(?: g=(\S*))?(?: local=(\d+))? info=(.*) addr=(.*)$"
)
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as f:
    for line in f:
        if "ESP-ROM" in line:
            now = {}
            continue
        if not (m := line_re.search(line.rstrip("\n"))):
            continue
        k = f"{m[1]} {m[2]}"
        if m[3] in (None, "G") and good_info.match(m[5]) and m[6] == addr:
            ok[k] += 1
        else:
            bad[k] += 1
            if shown < 15:
                print("BAD", line[:200].rstrip("\n"))
                shown += 1
        if m[4] is None:
            continue
        # pair each deferred read with the cap-now line of the same trigger run
        key = (m[1], m[4])
        if m[2] == "cap-now":
            now[key] = (m[5], m[6])
        elif key in now:
            (pair_ok if now[key] == (m[5], m[6]) else pair_bad)[k] += 1
        else:
            unpaired += 1
for k in sorted(set(ok) | set(bad)):
    print(f"{k:28s} ok={ok[k]} bad={bad[k]}")
print(
    "deferred value equal to the value in the trigger:",
    dict(pair_ok),
    "different:",
    dict(pair_bad),
    "without a cap-now line:",
    unpaired,
)
