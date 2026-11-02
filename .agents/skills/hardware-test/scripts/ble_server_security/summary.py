"""Judge one run.sh run from the client log (drive.py output) and the server's serial capture.

usage: summary.py MODE CLIENT_LOG SERVER_CAPTURE   MODE: jw (Just Works) or nc (numeric comparison, MITM)
Before the first Pair: the encrypted and MITM reads must each be tried and refused (an `Error reading char` reply),
no notification may arrive and no encrypted write may reach the server. After it: the encrypted read returns 99.9,
exactly one write (the paired one) reaches the server, the second Subscribe brings notifications, and the MITM-only
characteristic is readable only after an nc pairing. Prints the reconnect pattern and PASS or FAIL.
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


def outcome(lines: list[str], button: str, ok_pat: str) -> str:
    """'ok', 'refused' or 'none' for the first press of BUTTON.

    The window runs to the next read, pair or connection step, since a reply can come after a write press. A value
    wins over an error line: the error can belong to another characteristic read in the same window.
    """
    start = next((i for i, line in enumerate(lines) if f"PRESS {button}" in line), None)
    if start is None:
        return "none"
    window = []
    for line in lines[start + 1 :]:
        if re.search(r"PRESS (Read|Pair|Disconnect|Connect|Subscribe|Remove)", line):
            break
        window.append(line)
    if any(re.search(ok_pat, line) for line in window):
        return "ok"
    return "refused" if any("Error reading char" in line for line in window) else "none"


secure_before = outcome(before, "Read secure", r"secure read: 9")
mitm_before = outcome(before, "Read mitm", r"mitm read: m")
mitm_after = outcome(after, "Read mitm", r"mitm read: m")
writes = srv.count("secure write received")
checks = {
    f"encrypted read refused before pairing ({secure_before})": secure_before
    == "refused",
    f"MITM read refused before pairing ({mitm_before})": mitm_before == "refused",
    "no notifications before pairing": count(before, r"notification: n") == 0,
    "encrypted read after pairing": outcome(after, "Read secure", r"secure read: 99\.9")
    == "ok",
    # one Write secure press before Pair (must be refused) and one after it
    f"only the paired write reached the server ({writes})": writes == 1,
    "notifications after subscribing paired": count(after_sub, r"notification: n") > 0,
}
if MODE == "nc":
    checks[f"MITM read after nc pairing ({mitm_after})"] = mitm_after == "ok"
else:
    checks[f"MITM read still refused after Just Works ({mitm_after})"] = (
        mitm_after == "refused"
    )
for name, ok in checks.items():
    print(f"{'ok  ' if ok else 'FAIL'} {name}")
print(
    f"server: {srv.count('Authentication complete')} authentications, {srv.count('SRV numeric comparison')} numeric "
    f"comparisons, {srv.count('Deleting unpaired device')} bonds dropped by the stack"
)
print("PASS" if all(checks.values()) else "FAIL")
sys.exit(0 if all(checks.values()) else 1)
