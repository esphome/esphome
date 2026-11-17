"""extract.py SERIAL OUT: keep MARK, pronto dump, trigger, binary sensor and heap lines from the first MARK on,
with timestamps and source line numbers removed."""

import pathlib
import re
import sys

out, on = [], False
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as fh:
    for raw in fh:
        x = re.sub(r"^\[[0-9:.]+\]", "", raw.rstrip("\n"))
        m = re.match(r"\[(\w)\]\[([\w.]+)(?::\d+)?\]: ?(.*)$", x)
        if not m:
            continue
        lvl, tag, msg = m.groups()
        if "MARK tx" in msg:
            on = True
        if not on:
            continue
        if (
            tag in ("remote.pronto", "trig", "heap")
            or "MARK" in msg
            or "BS pronto" in msg
            or tag == "remote.nec"
        ):
            out.append(f"{tag}: {msg}")
pathlib.Path(sys.argv[2]).write_text("\n".join(out) + "\n", encoding="utf-8")
