"""Per upload window (start line to end, abort or error), count Wi-Fi retry lines and [W] lines.
Usage: windows.py DUT_LOG"""

import pathlib
import re
import sys

start = re.compile(r"OTA Update Start|Starting update from")
end = re.compile(r"OTAEV (web )?(end|abort|error)")
retry = re.compile(
    r"Starting scan|Connecting to|Retry phase|Network no longer found|No networks found"
    r"|Connecting to network failed"
)
cur = None


def incomplete(c: dict) -> None:
    print(
        f"{c['t0']}..? INCOMPLETE (no end line) retry_lines={c['retry']} W_lines={c['warn']}"
    )


with pathlib.Path(sys.argv[1]).open("rb") as f:
    for raw in f:
        line = raw.decode("utf-8", "replace").rstrip()
        ts = line[1:9]
        if start.search(line):
            if cur is not None:
                incomplete(cur)
            cur = {"t0": ts, "retry": 0, "warn": 0, "first": None, "last_prog": None}
            continue
        if cur is None:
            continue
        if retry.search(line):
            cur["retry"] += 1
            cur["first"] = cur["first"] or line[:90]
        if "][W][" in line:
            cur["warn"] += 1
        if "progress" in line.lower():
            cur["last_prog"] = ts
        if m := end.search(line):
            print(
                f"{cur['t0']}..{ts} {m.group(0):16} retry_lines={cur['retry']} W_lines={cur['warn']} "
                f"last_progress={cur['last_prog']} {cur['first'] or ''}"
            )
            cur = None
if cur is not None:
    incomplete(cur)
