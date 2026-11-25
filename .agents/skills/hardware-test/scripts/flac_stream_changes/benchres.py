"""Shared parser for the flacbench.hpp RES lines."""

import pathlib
import re

FILES = ["s16", "m16", "s24", "m24"]
RES = re.compile(
    r"RES r(\d+) (\w+) .*frames (\d+) dec_us (\d+) .*rtf ([\d.]+) fnv (\w+) err (\d+) "
    r"used_dram (-?\d+) used_iram (-?\d+) used_ps (-?\d+)"
)


def load(path: str) -> dict[str, list[re.Match]]:
    """RES matches per clip name."""
    rows: dict[str, list[re.Match]] = {}
    for ln in (
        pathlib.Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
    ):
        if m := RES.search(ln):
            rows.setdefault(m[2], []).append(m)
    return rows
