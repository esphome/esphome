"""Judge one run.sh run from the client log (drive.py output) and the server's serial capture.

usage: summary.py MODE CLIENT_LOG SERVER_CAPTURE   MODE: jw (Just Works) or nc (numeric comparison, MITM)
Before the first Pair: encrypted reads refused, no notifications, no encrypted write on the server. After it: the
encrypted read returns 99.9, the write reaches the server, the second Subscribe brings notifications, and the
MITM-only characteristic is readable only after an nc pairing. Prints the reconnect pattern and PASS or FAIL.
"""

import pathlib
import re
import sys

MODE = sys.argv[1]
cli = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8").splitlines()
srv = re.sub(
    r"\x1b\[[0-9;]*m",
    "",
    pathlib.Path(sys.argv[3]).read_bytes().decode(errors="replace"),
)
pair_at = next((i for i, line in enumerate(cli) if "PRESS Pair" in line), None)
if pair_at is None:
    sys.exit("FAIL: no Pair step in the client log")
before, after = cli[:pair_at], cli[pair_at:]
subs = [i for i, line in enumerate(after) if "PRESS Subscribe" in line]
after_sub = after[subs[0] :] if subs else []


def count(lines: list[str], pat: str) -> int:
    return sum(1 for line in lines if re.search(pat, line))


checks = {
    "encrypted read refused before pairing": count(before, r"secure read: 9") == 0,
    "no notifications before pairing": count(before, r"notification: n") == 0,
    "encrypted read after pairing": count(after, r"secure read: 99\.9") > 0,
    "encrypted write reached the server": "secure write received" in srv,
    "notifications after subscribing paired": count(after_sub, r"notification: n") > 0,
    "MITM characteristic refused before pairing": count(before, r"mitm read: m") == 0,
}
mitm_after = count(after, r"mitm read: m") > 0
checks[
    "MITM characteristic "
    + (
        "readable after nc pairing"
        if MODE == "nc"
        else "still refused after Just Works"
    )
] = mitm_after if MODE == "nc" else not mitm_after
for name, ok in checks.items():
    print(f"{'ok  ' if ok else 'FAIL'} {name}")
print(
    f"server: {srv.count('Authentication complete')} authentications, {srv.count('SRV numeric comparison')} numeric "
    f"comparisons, {srv.count('Deleting unpaired device')} bonds dropped by the stack"
)
print("PASS" if all(checks.values()) else "FAIL")
sys.exit(0 if all(checks.values()) else 1)
