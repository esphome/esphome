"""nan.py CAPTURE...: no DNS server case before the first Wi-Fi drop: lookup retry gaps and on_disconnect reasons."""

from datetime import UTC, datetime
import pathlib
import re
import sys

CR = "Couldn't resolve"
for f in sys.argv[1:]:
    t = pathlib.Path(f).read_text(encoding="utf-8", errors="replace")
    # stop at the first Wi-Fi drop
    cut = t.find("EV DROP")
    pre = t if cut < 0 else t[:cut]
    er = [
        datetime.strptime(m[1], "%H:%M:%S.%f").replace(tzinfo=UTC)
        for m in re.finditer(
            r"\[(\d\d:\d\d:\d\d\.\d+)\]\[W\]\[mqtt:\d+\]: Error resolving", pre
        )
    ]
    gaps = [(b - a).total_seconds() for a, b in zip(er, er[1:], strict=False)]
    downs = re.findall(r"EV MQTT DOWN \d+ reason=(\d+)", pre)
    disc = re.findall(r"Disconnected: ([A-Za-z ]+)", pre)
    print(
        f"{f}: wifi_up={'EV WIFI UP' in pre} err_resolving={len(er)} couldnt={pre.count(CR)} "
        f"on_disconnect={len(downs)} reasons={sorted(set(downs))} disc_msgs={ {d: disc.count(d) for d in set(disc)} } "
        f"gap={f'{min(gaps):.2f}..{max(gaps):.2f}' if gaps else '-'} mqtt_up={'EV MQTT UP' in t} "
        f"resets={len(re.findall('rst:0x|ets Jan', t))} alive={t.count('EV ALIVE')} panic={len(re.findall('Guru|abort\\(|Exception|wdt', t))}"
    )
