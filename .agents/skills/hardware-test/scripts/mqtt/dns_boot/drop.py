"""drop.py CAPTURE...: Wi-Fi and MQTT events around each Wi-Fi drop, and MQTT up after Wi-Fi up and wifi.enable."""

import pathlib
import re
import sys

CR = "Couldn't resolve"
for f in sys.argv[1:]:
    t = pathlib.Path(f).read_text(encoding="utf-8", errors="replace")
    ev = re.findall(r"EV (WIFI UP|WIFI DOWN|MQTT UP|MQTT DOWN|DROP|RESTORE) (\d+)", t)
    print(
        f"== {f} couldnt={t.count(CR)} errres={t.count('Error resolving')} lookups={t.count('Resolving broker')} "
        f"boots={t.count('Enabling')}"
    )
    restore = None
    wup = None
    for k, ms in ev:
        ms = int(ms)
        if k == "RESTORE":
            restore = ms
        if k == "WIFI UP":
            wup = ms
        if k == "MQTT UP" and wup is not None:
            print(
                f"   MQTT UP {ms} : {ms - wup} ms after Wi-Fi up"
                + (f", {ms - restore} ms after wifi.enable" if restore else "")
            )
        if k != "MQTT UP":
            print(f"   {k} {ms}")
