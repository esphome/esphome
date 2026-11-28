"""Per phase IPv6 link-local and SLAAC statistics from v6probe captures.

usage: an.py CAPTURE...
For the boot and reconnect phases: boots, boots with an esp_netif_create_ip6_linklocal failure, boots without
link-local, SLAAC or a connection, connection timeouts, and median/worst times in seconds. Counts missing
addresses from the RESULT line too, since the USB console can drop a warning line. Exits 1 when a boot other
than the last of a capture has no RESULT for a phase.
"""

import pathlib
import re
import statistics
import sys


def parse(paths: list[str]) -> list[dict]:
    boots: list[dict] = []
    cur: dict | None = None
    for p in paths:
        for line in pathlib.Path(p).read_bytes().decode(errors="replace").splitlines():
            if "BOOTMARK" in line:
                cur = {
                    "boot": {},
                    "reconnect": {},
                    "fail": [0, 0],
                    "created": [0, 0],
                    "timeouts": [0, 0],
                    "ph": 0,
                }
                boots.append(cur)
                continue
            if cur is None:
                continue
            ph = cur["ph"]
            if "esp_netif_create_ip6_linklocal failed" in line:
                cur["fail"][ph] += 1
            if "IPv6 link-local address created" in line or "No lwIP netif" in line:
                cur["created"][ph] += 1
            if re.search(
                r"Connection timeout|Connection incomplete|Connection lost", line
            ):
                cur["timeouts"][ph] += 1
            if "REENABLE" in line:
                cur["ph"] = 1
            if m := re.search(r"RESULT (\w+) (.*)", line):
                cur[m.group(1)] = dict(kv.split("=") for kv in m.group(2).split())
        if cur is not None:
            cur["last"] = True
        cur = None
    return boots


def med_max(xs: list[int]) -> str:
    return f"{statistics.median(xs) / 1000:.1f}/{max(xs) / 1000:.1f}" if xs else "-"


def summ(boots: list[dict], phase: str, idx: int) -> int:
    rs = [b for b in boots if b[phase]]
    # a capture usually ends inside its last boot
    no_result = sum(1 for b in boots if not b[phase] and "last" not in b)

    def seen(key: str) -> list[int]:
        return [int(b[phase][key]) for b in rs if b[phase][key] != "0"]

    def missing(key: str) -> int:
        return sum(1 for b in rs if b[phase][key] == "0")

    print(
        f"{phase:9} n={len(rs)} no_result={no_result} esp_fail={sum(1 for b in rs if b['fail'][idx])} no_ll={missing('ll')} "
        f"no_slaac={missing('gl')} not_connected={missing('full')} timeouts={sum(b['timeouts'][idx] for b in rs)} "
        f"ll(med/max s)={med_max(seen('ll'))} slaac={med_max(seen('gl'))} connected={med_max(seen('full'))} "
        f"created_logs={sum(b['created'][idx] for b in rs)}"
    )
    return no_result


def main() -> None:
    boots = parse(sys.argv[1:])
    print("boots", len(boots))
    no_result = summ(boots, "boot", 0) + summ(boots, "reconnect", 1)
    print(
        "heap_used first 3 boots",
        [int(b["boot"]["heap_used"]) for b in boots if b["boot"]][:3],
    )
    if no_result or not boots:
        sys.exit(1)


if __name__ == "__main__":
    main()
