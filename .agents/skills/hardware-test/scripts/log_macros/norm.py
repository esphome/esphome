"""norm.py SERIAL: config lines ([C] and indented follow-ups), lm lines, sensor/switch/fan/light/lock state lines.

Volatile values are masked, not dropped, so a line that changes between builds still shows in the diff: addresses,
the device name (from the Hostname line), signal, setup times, and the value of keys that change per boot or per build.
"""

import pathlib
import re
import sys

out = []
cfg = False
MASKS = (
    (re.compile(r"\b(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}\b"), "<mac>"),
    (re.compile(r"\b\d{1,3}(?:\.\d{1,3}){3}\b"), "<ip>"),
    (re.compile(r"-?\d+(?:\.\d+)? ?dBm?\b.*"), "<dB>"),
    (re.compile(r"\btook \d+ ?ms\b"), "took <t>"),
)
# keys whose value changes per boot, per board or per build; the key stays, the value becomes <v>
VOLATILE_KEY = re.compile(
    r"^((?:\w [\w.]+: )?\s*(?:Channel|Unsuccessful boot attempts|Reset reason|Wakeup cause|Free heap|Heap|"
    r"Uptime|Compiled|Compilation time|Config hash|Build|ESPHome version|Flash size|CPU frequency|Chip):\s*)\S.*$",
    re.IGNORECASE,
)
name = None
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
        if name is None and (m := re.search(r"Hostname: '?([\w-]+)'?$", line)):
            name = m.group(1)
        line = VOLATILE_KEY.sub(r"\1<v>", line)
        for rx, repl in MASKS:
            line = rx.sub(repl, line)
        out.append(line)
text = "\n".join(out)
if name:
    text = re.sub(rf"\b{re.escape(name)}\b", "<name>", text)
print(text)
if not out:
    sys.exit(f"no config, lm or state lines in {sys.argv[1]}")
