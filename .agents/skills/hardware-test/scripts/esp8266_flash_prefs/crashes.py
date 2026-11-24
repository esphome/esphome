"""List the exceptions and hardware watchdog resets in a scan-repro capture, with the last heap line before each.

usage: crashes.py CAPTURE FIRMWARE_ELF
Decodes each exception PC with xtensa-lx106-elf-addr2line (on PATH, or set ADDR2LINE to the toolchain's binary).
"""

import os
import pathlib
import re
import subprocess
import sys

log, elf = sys.argv[1], sys.argv[2]
A = os.environ.get("ADDR2LINE", "xtensa-lx106-elf-addr2line")
HEAP_RE = re.compile(
    r"free heap (\d+), min (\d+), max block (\d+), done (\d+), up (\d+)"
)
with pathlib.Path(log).open(encoding="utf-8", errors="replace") as fh:
    lines = fh.read().splitlines()


def sym(a: str) -> str:
    out = subprocess.run(
        [A, "-fiaC", "-e", elf, a], capture_output=True, text=True, check=False
    ).stdout.split("\n")
    return out[1] if len(out) > 1 else "?"


last = None
for i, ln in enumerate(lines):
    if "repro:" in ln:
        last = ln
    m = re.search(r"Exception \((\d+)\)", ln)
    if not m and "rst cause:4" not in ln:
        continue
    if (
        m
        and i + 1 < len(lines)
        and (ep := re.search(r"epc1=(0x\w+).*excvaddr=(0x\w+)", lines[i + 1]))
    ):
        ctx = next((x for x in lines[i : i + 8] if "ctx:" in x), "")
        pc, ev = ep.group(1), ep.group(2)
        print(f"{ln[:10]} exc {m.group(1)} pc {pc} {sym(pc)} excvaddr {ev} {ctx[10:]}")
    else:
        print(
            f"{ln[:10]} hardware WDT (rst cause 4)"
            if not m
            else f"{ln[:10]} exc {m.group(1)}"
        )
    if last and (h := HEAP_RE.search(last)):
        print(
            f"    last repro {last[:10]} heap {h.group(1)} maxblock {h.group(3)} up {h.group(5)}"
        )
    last = None
