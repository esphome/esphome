"""lognorm.py [FILE...]: print ESPHome log lines without ANSI colours, the capture's `[hh:mm:ss]` or
`[hh:mm:ss.mmm]` timestamp and the `:NNN` source line number of the tag, so two builds' logs can be diffed.

Reads stdin when no file is given. Scripts can import normalize() instead.
"""

import fileinput
import re

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
STAMP = re.compile(r"^\[\d\d:\d\d:\d\d(?:\.\d+)?\]")
LINENO = re.compile(r"(\[[A-Z]\]\[[\w.\-]+):\d+\]")


def normalize(line: str) -> str:
    return LINENO.sub(r"\1]", STAMP.sub("", ANSI.sub("", line)))


if __name__ == "__main__":
    with fileinput.input(encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            print(normalize(raw.rstrip("\n")))
