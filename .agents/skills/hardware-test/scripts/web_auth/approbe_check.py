"""approbe_check.py CLIENT_LOG: verdict for the probes of configs/ap-client-common.yaml in the client's serial log.

Needs at least one round of all four probes and the same number of replies to each (a cut-off round fails); in every
round a POST without credentials must get 401 and one with credentials 200, and the portal root and /config.json must
answer (any status but -1; their codes are printed to compare two builds). Exits 1 when a probe is missing or a status
is wrong.
"""

import pathlib
import re
import sys

RX = re.compile(r"probe (portal_root|config_json|post_noauth|post_auth) status (-?\d+)")
WANT = {"post_noauth": "401", "post_auth": "200"}

seen: dict[str, list[str]] = {
    k: [] for k in ("portal_root", "config_json", "post_noauth", "post_auth")
}
for line in (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
):
    if m := RX.search(line):
        seen[m[1]].append(m[2])
fails = []
if len({len(c) for c in seen.values()}) > 1:
    fails.append("unequal probe counts, a round did not finish")
for name, codes in seen.items():
    print(f"{name}: {codes}")
    if not codes:
        fails.append(f"{name} never ran")
    elif name in WANT and any(c != WANT[name] for c in codes):
        fails.append(f"{name} got {codes}, want {WANT[name]}")
    elif "-1" in codes:
        fails.append(f"{name} got no answer")
print("PASS" if not fails else "FAIL: " + "; ".join(fails))
sys.exit(1 if fails else 0)
