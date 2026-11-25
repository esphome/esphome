"""List the exceptions and soft and hardware watchdog resets in a scan-repro capture, with the last heap line before each, or
with --trend the heap per boot: lowest heap and largest block, and the heap about once a minute.

usage: crashes.py CAPTURE FIRMWARE_ELF
       crashes.py --trend CAPTURE
Exception PCs are decoded with the xtensa-lx106-elf-addr2line found in ~/.platformio, or the one ADDR2LINE names.
"""

import os
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from heap_alloc.decode import decode, find_addr2line  # noqa: E402
from lognorm import STAMP  # noqa: E402

HEAP_RE = re.compile(r"free heap (\d+), max block (\d+), done (\d+), up (\d+)")
EXC_RE = re.compile(r"Exception \((\d+)\)")
EPC_RE = re.compile(r"epc1=(0x\w+).*excvaddr=(0x\w+)")


def stamp(ln: str) -> str:
    return m.group(0) if (m := STAMP.match(ln)) else ""


def trend(lines: list[str]) -> None:
    boots = []
    cur = None
    for ln in lines:
        if not (m := HEAP_RE.search(ln)):
            continue
        h, mb, up = int(m.group(1)), int(m.group(2)), int(m.group(4))
        if cur is None or up < cur[-1][2]:
            cur = []
            boots.append((stamp(ln), cur))
        cur.append((h, mb, up))
    for t, s in boots:
        pts = [f"{p[2]}s:{p[0] // 1024}k" for p in s if p[2] % 60 < 3 or p == s[-1]]
        print(
            t,
            f"min {min(p[0] for p in s)} minblock {min(p[1] for p in s)} last up {s[-1][2]}",
            " ".join(pts),
        )


def crashes(lines: list[str], elf: str) -> None:
    found = []
    last = None
    for i, ln in enumerate(lines):
        if "repro:" in ln:
            last = ln
        m = EXC_RE.search(ln)
        wdt = (
            "Soft WDT reset" in ln
            or "rst cause:4" in ln
            or "Reason: Hardware WDT" in ln
        )
        if not m and not wdt:
            continue
        ep = EPC_RE.search(lines[i + 1]) if m and i + 1 < len(lines) else None
        ctx = next((x for x in lines[i : i + 8] if "ctx:" in x), "") if ep else ""
        found.append((ln, m, ep, STAMP.sub("", ctx), last))
        last = None
    pcs = sorted({f"{int(ep.group(1), 16):08x}" for _, _, ep, _, _ in found if ep})
    names = {}
    if pcs:
        a2l = os.environ.get("ADDR2LINE") or find_addr2line(
            "xtensa-lx106-elf-addr2line"
        )
        names = decode(a2l, elf, pcs)
    for ln, m, ep, ctx, last in found:
        t = stamp(ln)
        if ep:
            frames = names.get(f"{int(ep.group(1), 16):08x}") or [("?", "")]
            print(
                f"{t} exc {m.group(1)} pc {ep.group(1)} {frames[0][0]} excvaddr {ep.group(2)} {ctx}"
            )
        else:
            print(
                f"{t} exc {m.group(1)}"
                if m
                else f"{t} {'soft' if 'Soft WDT' in ln else 'hardware'} WDT"
            )
        if last and (h := HEAP_RE.search(last)):
            print(
                f"    last repro {stamp(last)} heap {h.group(1)} maxblock {h.group(2)} up {h.group(4)}"
            )


if __name__ == "__main__":
    args = sys.argv[1:]
    mode_trend = args[0] == "--trend"
    with pathlib.Path(args[1] if mode_trend else args[0]).open(
        encoding="utf-8", errors="replace"
    ) as fh:
        text = fh.read().splitlines()
    if mode_trend:
        trend(text)
    else:
        crashes(text, args[1])
