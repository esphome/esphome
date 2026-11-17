"""rec_extract.py CAPTURE OUT: turn the "REC ..." log lines of sxrec.hpp into a record file for compare_spi.py."""

import pathlib
import re
import sys

out = []
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as fh:
    for line in fh:
        m = re.search(r"REC (.*)$", line.rstrip())
        if not m:
            continue
        body = m.group(1)
        if body.startswith("+"):
            out[-1] += body[1:]
        else:
            out.append(body)
pathlib.Path(sys.argv[2]).write_text("\n".join(out) + "\n", encoding="utf-8")
