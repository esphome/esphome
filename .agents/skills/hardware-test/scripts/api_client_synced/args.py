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
shown = 0
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as f:
    lines = [line.rstrip("\n") for line in f]
line_re = re.compile(
    r"\]: (CONNECTED|DISCONNECTED|SYNCED) (direct|delayed1?|cap-now|cap-later|cap-copied)"
    r"(?: g=G)?(?: local=\d+)? info=(.*) addr=(.*)$"
)
for line in lines:
    if not (m := line_re.search(line)):
        continue
    k = f"{m[1]} {m[2]}"
    if good_info.match(m[3]) and m[4] == addr:
        ok[k] += 1
    else:
        bad[k] += 1
        if shown < 15:
            print("BAD", line[:200])
            shown += 1
for k in sorted(set(ok) | set(bad)):
    print(f"{k:28s} ok={ok[k]} bad={bad[k]}")
now = {}
pair_ok = collections.Counter()
pair_bad = collections.Counter()
cap_re = re.compile(
    r"\]: (\w+) cap-(now|later|copied)(?: g=G)? local=(\d+) info=(.*) addr=(.*)$"
)
for line in lines:
    if "ESP-ROM" in line:
        now = {}
        continue
    if not (m := cap_re.search(line)):
        continue
    key = (m[1], m[3])
    if m[2] == "now":
        now[key] = (m[4], m[5])
    elif key in now:
        (pair_ok if now[key] == (m[4], m[5]) else pair_bad)[f"{m[1]} {m[2]}"] += 1
print(
    "deferred value equal to the value in the trigger:",
    dict(pair_ok),
    "different:",
    dict(pair_bad),
)
