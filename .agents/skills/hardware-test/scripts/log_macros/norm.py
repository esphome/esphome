"""norm.py SERIAL: config lines ([C] and indented follow-ups), lm lines, sensor/switch/fan/light/lock state lines; strip volatile bits."""

import pathlib
import re
import sys

out = []
cfg = False
VOL = re.compile(
    r"IP Address|MAC|BSSID|Hostname|Signal|Channel|Gateway|Subnet|DNS|compiled|Compile|Uptime|heap|Heap|Free|RSSI|Scan Results|SSID|Address:|Subnet|Reset reason|Boot|Version|ESPHome version|Loop|loop|ms\b|Config hash|Up ?time|frag|Max|stack|Flash|Chip|CPU|MHz|Wakeup|bootloader|SDK|Core|reason|hash|Power|IDF|Mbps|Tx|Partition"
)
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as fh:
    for raw in fh:
        x = re.sub(r"^\[[0-9:.]+\]", "", raw.rstrip("\n"))
        m = re.match(r"\[(\w)\]\[([\w.]+)(?::\d+)?\]: ?(.*)$", x)
        if m:
            lvl, tag, msg = m.groups()
            cfg = lvl == "C"
            if (
                lvl == "C"
                or tag == "lm"
                or re.search(r"State|state|Setting|Sending|Turning|'.*'", msg)
                and tag
                in (
                    "sensor",
                    "switch",
                    "fan",
                    "light",
                    "lock",
                    "binary_sensor",
                    "number",
                    "alarm_control_panel",
                    "template.alarm_control_panel",
                )
            ):
                line = f"{lvl} {tag}: {msg}"
            else:
                continue
        elif cfg and x.startswith(" "):
            line = "    " + x.strip()
        else:
            continue
        if VOL.search(line):
            continue
        out.append(line)
print("\n".join(out))
