"""ana.py CAPTURE: reading counts, median, NaN, warnings, failures and boots, then the distinct register log lines."""

import pathlib
import re
import statistics
import sys

t = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
for n in ("Distance", "Illuminance"):
    v = re.findall(rf"'{n}' >> (\S+)", t)
    f = [float(x) for x in v if x != "nan"]
    stats = (
        f"median={statistics.median(f)} min={min(f)} max={max(f)} stdev={statistics.pstdev(f):.2f}"
        if f
        else ""
    )
    print(f"{n}: n={len(v)} nan={len(v) - len(f)}", stats)
print(
    "W", len(re.findall(r"\[W\]", t)),
    "E", len(re.findall(r"\[E\]", t)),
    "warnflag set", len(re.findall(r"set Warning flag", t)),
    "failed", len(re.findall(r"marked FAILED|mark_failed", t)),
    "boots", len(re.findall(r"Running through setup", t)),
)  # fmt: skip
for line in sorted(set(re.findall(r"regs:\d+\]: (.*)", t))):
    print(" ", line)
