"""ana.py NAME...: per boot capture the Wi-Fi up to MQTT up time and the broker lookup errors.

Reads NAME-bootK.txt next to this script, where boots.sh writes them.
"""

import pathlib
import re
import sys

CR = "Couldn't resolve"
D = pathlib.Path(__file__).resolve().parent
for name in sys.argv[1:]:
    print(f"== {name}")
    for f in sorted(D.glob(f"{name}-boot*.txt")):
        t = f.read_text(encoding="utf-8", errors="replace")
        w = re.search(r"EV WIFI UP (\d+)", t)
        m = re.search(r"EV MQTT UP (\d+)", t)
        pre = t[: m.start()] if m else t
        delta = int(m[1]) - int(w[1]) if (w and m) else None
        print(
            f"{f.name}: wifi={w and w[1]} mqtt={m and m[1]} delta_ms={delta} "
            f"err_resolving={t.count('Error resolving')} couldnt={t.count(CR)} "
            f"down_before_up={pre.count('EV MQTT DOWN')} down_total={t.count('EV MQTT DOWN')} "
            f"resolving={t.count('Resolving broker')} enabling={t.count('Enabling')}"
        )
