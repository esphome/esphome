"""Decode an allocring dump: decode.py LOG ELF [ADDR2LINE]

Prints one block per entry (time, size, then each caller frame as function and file:line), and markers
(size 0) as phase separators. ADDR2LINE defaults to the first xtensa or riscv addr2line found under
~/.platformio and ~/.espressif.
"""

from pathlib import Path
import re
import subprocess
import sys

ENTRY = re.compile(r"\bE (\d+) (\d+) (\d+) ((?:[0-9a-f]{8} ?){5})")


def find_addr2line() -> str:
    for root in (Path.home() / ".platformio", Path.home() / ".espressif"):
        for pattern in ("**/xtensa-esp*-elf-addr2line", "**/riscv32-esp-elf-addr2line"):
            for path in root.glob(pattern):
                if path.is_file():
                    return str(path)
    sys.exit("no addr2line found; pass it as the third argument")


def short(path: str) -> str:
    for marker in ("esphome/components/", "esphome/core/", "esp-idf/", "/include/c++/"):
        if marker in path:
            return marker.lstrip("/") + path.split(marker, 1)[1]
    return path


def main() -> None:
    log, elf = sys.argv[1], sys.argv[2]
    addr2line = sys.argv[3] if len(sys.argv) > 3 else find_addr2line()
    entries = [
        m.groups()
        for m in ENTRY.finditer(Path(log).read_text(encoding="utf-8", errors="replace"))
    ]
    pcs = sorted(
        {pc for e in entries if e[2] != "0" for pc in e[3].split() if pc != "00000000"}
    )
    # One call per address: with -i an address can print several inlined frames.
    names = {}
    for pc in pcs:
        lines = subprocess.run(
            [addr2line, "-fiC", "-e", elf, f"0x{pc}"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.splitlines()
        names[pc] = [
            (lines[j], short(lines[j + 1])) for j in range(0, len(lines) - 1, 2)
        ]
    for idx, ms, size, frames in entries:
        if size == "0":
            a, b = frames.split()[:2]
            print(f"--- marker at {int(ms) / 1000:.3f} s: {int(a, 16)} {int(b, 16)}")
            continue
        print(f"#{idx} {int(ms) / 1000:.3f} s {size} B")
        for pc in frames.split():
            if pc == "00000000":
                break
            for func, loc in names.get(pc, [("??", "")]):
                print(f"    {func[:110]}  {loc}")


if __name__ == "__main__":
    main()
