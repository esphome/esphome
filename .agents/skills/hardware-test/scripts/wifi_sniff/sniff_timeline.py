"""sniff_timeline.py CAPTURE [MAC=LABEL ...]: Null Data frames per destination for each 5 s SNIF window.

Labels make the output readable (the AP's BSSID, the sniffer's own MAC); `(ToDS)` marks frames sent to the AP side.
Use FC=xx to count another frame control byte (default 48, Null Data).
"""

import pathlib
import re
import sys

names = {}
fc_want = 0x48
for a in sys.argv[2:]:
    k, _, v = a.partition("=")
    if k == "FC":
        fc_want = int(v, 16)
    else:
        names[k.lower()] = v
text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
for line in text.splitlines():
    if "SNIF" not in line:
        continue
    out = {}
    for m in re.finditer(r"([0-9a-f:]{17})=(\d+) fc=([0-9a-f]{4})", line):
        fc = int(m.group(3), 16)
        if fc & 0xFF != fc_want:
            continue
        k = names.get(m.group(1), m.group(1)) + ("(ToDS)" if fc & 0x100 else "")
        out[k] = out.get(k, 0) + int(m.group(2))
    ts = m.group(1) if (m := re.match(r"\[([^\]]+)\]", line)) else "-"
    print(ts, " ".join(f"{k}={v}" for k, v in sorted(out.items())))
