"""summary.py TAG:SCENARIO[:SLOT]...: API time (connect request to services, resolved attempts), device link-up and
discovery-only (devtime.py over the scenario window), min/med/p95/max; SLOT limits the device numbers to one slot."""

import sys

from stats import api_times, devtime, fmt, link_and_discovery, rows

for arg in sys.argv[1:]:
    tag, sc, *slot = arg.split(":")
    rs = rows(tag, sc)
    link, disc = link_and_discovery(devtime(tag, rs), slot[0] if slot else None)
    print(
        f"{tag} {sc}: API {fmt(api_times(rs))} | link-up {fmt(link)} | discovery {fmt(disc)}"
    )
