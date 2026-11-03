"""Summarize an lddrive.py log: for each step, the LD2450 TX frames, commands and publishes.

usage: txsum.py LOGFILE   (diff the output of two builds)
"""

from pathlib import Path
import re
import sys

ANSI = re.compile(r"\x1b\[[0-9;]*m")
out: list[str] = []
with Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as log:
    for raw in log:
        line = ANSI.sub("", raw.rstrip("\n"))
        m = re.match(r"\s*[\d.]+ (.*)", line)
        if not m:
            continue
        text = m.group(1)
        if text.startswith(">>>"):
            out.append(f"\n# {text[4:]}")
        elif "[uart_debug" in text:
            out.append(f"  TX {text.split('>>> ')[1]}")
        elif "[ld2450" in text and (
            "Sending COMMAND" in text
            or "zone type" in text.lower()
            or "Query zone" in text
        ):
            out.append(f"  LOG {text.split(']: ', 1)[1]}")
        elif "[select:" in text and ">>" in text:
            out.append(f"  SEL {text.split(']: ', 1)[1]}")
        elif "[text_sensor:098]" in text:
            out.append(f"  TXT {text.split(']: ', 1)[1]}")
print("\n".join(out))
