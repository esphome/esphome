"""devtime.py CAPTURE [FROM_HHMMSS] [TO_HHMMSS]: per proxy slot attempt, device-side ms from 'Connecting' to the link
(IVLOG connect) and to 'Discovery finished', plus the interval in the connect event (needs ivlog.hpp); no addresses
printed."""

import pathlib
import re
import sys


def ts(line: str) -> float | None:
    """Seconds of day of a [HH:MM:SS.mmm] stamped capture line."""
    m = re.match(r"\[(\d+):(\d+):(\d+)\.(\d+)\]", line)
    return (
        int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]) + int(m[4]) / 1000 if m else None
    )


def attempts(cap: str, lo: str = "00:00:00", hi: str = "99:99:99") -> list[str]:
    """One line per attempt in CAPTURE between LO and HI."""
    out: list[str] = []
    cur: dict[str, dict] = {}
    with pathlib.Path(cap).open(encoding="utf-8", errors="replace") as f:
        for line in f:
            if not line.startswith("[") or not (lo <= line[1:9] <= hi):
                continue
            t = ts(line)
            if t is None:
                continue
            if m := re.search(r"\]: \[(\d)\] 0x\w+ Connecting", line):
                cur[m[1]] = {"t0": t, "slot": m[1]}
                continue
            if m := re.search(r"IVLOG connect \S+ if=3 interval=(\d+)", line):
                for c in cur.values():
                    if "tc" not in c:
                        c["tc"] = t
                        c["iv"] = int(m[1])
                continue
            if (
                m := re.search(r"\]: \[(\d)\] \[[^]]*\] Discovery finished", line)
            ) and m[1] in cur:
                c = cur.pop(m[1])
                tc = c.get("tc", t)
                out.append(
                    f"{line[1:13]} slot{c['slot']} iv={c.get('iv')} link={1000 * (tc - c['t0']):.0f}ms "
                    f"discovery={1000 * (t - tc):.0f}ms total={1000 * (t - c['t0']):.0f}ms"
                )
            elif (
                m := re.search(
                    r"\]: \[(\d)\] Connection open failed, status=(\d+)", line
                )
            ) and m[1] in cur:
                c = cur.pop(m[1])
                out.append(
                    f"{line[1:13]} slot{c['slot']} FAILED status={m[2]} after {1000 * (t - c['t0']):.0f}ms"
                )
            elif (
                m := re.search(
                    r"\]: \[(\d)\] \[[^]]*\] Disconnected, reason=(0x\w+)", line
                )
            ) and m[1] in cur:
                c = cur.pop(m[1])
                out.append(
                    f"{line[1:13]} slot{c['slot']} iv={c.get('iv')} NOT RESOLVED, disconnected reason={m[2]} "
                    f"{1000 * (t - c['t0']):.0f}ms after Connecting"
                )
    return out


if __name__ == "__main__":
    for ln in attempts(*sys.argv[1:4]):
        print(ln)
