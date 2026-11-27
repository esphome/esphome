"""cad.py CAPTURE...: gaps between `Resolving broker` lines (lookup cadence)."""

from datetime import UTC, datetime
import pathlib
import re
import sys

for f in sys.argv[1:]:
    ts = [
        datetime.strptime(m[1], "%H:%M:%S.%f").replace(tzinfo=UTC)
        for m in re.finditer(
            r"\[(\d\d:\d\d:\d\d\.\d+)\]\[D\]\[mqtt:\d+\]: Resolving broker",
            pathlib.Path(f).read_text(encoding="utf-8", errors="replace"),
        )
    ]
    d = [(b - a).total_seconds() for a, b in zip(ts, ts[1:], strict=False)]
    if d:
        print(
            f"{f}: lookups={len(ts)} gap min={min(d):.2f}s median={sorted(d)[len(d) // 2]:.2f}s max={max(d):.2f}s"
        )
