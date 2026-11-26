"""rsan.py rs-TAG-drv.txt CAPTURE...: broker restart and Wi-Fi drop marks, MQTT events and messages per connection."""

from datetime import UTC, datetime
import pathlib
import re
import sys


def ts(s):
    return datetime.strptime(s, "%H:%M:%S.%f").replace(tzinfo=UTC)


drv = {
    ln.split()[2]: ts(ln.split()[0])
    for ln in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
}
for f in sys.argv[2:]:
    lines = pathlib.Path(f).read_text(encoding="utf-8", errors="replace").splitlines()
    ev = []
    for ln in lines:
        m = re.match(
            r"\[(\d\d:\d\d:\d\d\.\d+)\].*EV (MSG|MQTT UP|MQTT DOWN|WIFI UP|WIFI DOWN|DROP|RESTORE)",
            ln,
        )
        if m:
            ev.append((ts(m[1]), m[2], ln))
    print(f"== {f}")
    marks = [("broker stop", drv["STOP"]), ("broker start", drv["START"])] + [
        (k.lower(), t) for t, k, _ in ev if k in ("DROP", "RESTORE")
    ]
    marks.sort(key=lambda x: x[1])
    for name, t in marks:
        print(f"  {t:%H:%M:%S.%f}"[:-3], name)
    for t, k, ln in ev:
        if k in ("MQTT UP", "MQTT DOWN", "WIFI UP", "WIFI DOWN"):
            print(f"  {t:%H:%M:%S.%f}"[:-3], ln.split("]: ")[-1])
    # messages per segment between connects
    seg = []
    cur = 0
    for _, k, _ln in ev:
        if k == "MQTT UP":
            seg.append(cur)
            cur = 0
        if k == "MSG":
            cur += 1
    seg.append(cur)
    print("  messages received per connection (before first connect first):", seg)
    print(
        "  couldnt:",
        sum("Couldn't resolve" in ln for ln in lines),
        "err_res:",
        sum("Error resolving" in ln for ln in lines),
        "panic:",
        sum(
            bool(re.search("Guru|abort\\(|Exception \\(|wdt reset|Soft WDT", ln))
            for ln in lines
        ),
    )
