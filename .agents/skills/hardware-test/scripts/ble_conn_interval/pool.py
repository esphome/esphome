"""pool.py LABEL TAG:SCENARIO...: pooled device-side discovery and API time (min/med/p95/max) over several runs."""

import sys

from stats import api_times, devtime, fmt, link_and_discovery, rows

api: list[float] = []
disc: list[float] = []
for arg in sys.argv[2:]:
    tag, sc = arg.split(":")
    try:
        rs = rows(tag, sc)
    except FileNotFoundError:
        continue
    if not rs:
        continue
    api += api_times(rs)
    disc += link_and_discovery(devtime(tag, rs))[1]
print(f"{sys.argv[1]}: discovery {fmt(disc)} | API {fmt(api)}")
