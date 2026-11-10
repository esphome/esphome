"""Decode an allocring dump into callers: decode.py LOG ELF [ADDR2LINE]

Prints each entry with its caller frames as function and file:line, and markers (size 0) as phase separators.
ADDR2LINE defaults to one for the ELF's CPU found under ~/.platformio or ~/.espressif.
"""

from pathlib import Path
import re
import subprocess
import sys

ENTRY = re.compile(r"\bE (\d+) (\d+) (\d+) ((?:[0-9a-f]{8} ?){5})")
# ELF e_machine values
TOOL_FOR_MACHINE = {94: "xtensa-esp*-elf-addr2line", 243: "riscv32-esp-elf-addr2line"}


def find_addr2line(elf: str) -> str:
    machine = int.from_bytes(Path(elf).read_bytes()[18:20], "little")
    pattern = TOOL_FOR_MACHINE.get(machine)
    if pattern is None:
        sys.exit(f"unknown ELF machine {machine}; pass ADDR2LINE")
    for root in (Path.home() / ".platformio", Path.home() / ".espressif"):
        for path in root.glob(f"**/bin/{pattern}"):
            if path.is_file():
                return str(path)
    sys.exit("no addr2line found; pass it as the third argument")


def short(path: str) -> str:
    for marker in ("esphome/components/", "esphome/core/", "esp-idf/", "include/c++/"):
        if marker in path:
            return marker + path.split(marker, 1)[1]
    return path


def decode(
    addr2line: str, elf: str, pcs: list[str]
) -> dict[str, list[tuple[str, str]]]:
    """Every PC's frames (inlined ones too); -a prints each address before its frames."""
    out = subprocess.run(
        [addr2line, "-afiC", "-e", elf, *(f"0x{pc}" for pc in pcs)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.splitlines()
    names: dict[str, list[tuple[str, str]]] = {}
    frames: list[tuple[str, str]] = []
    i = 0
    while i < len(out):
        if out[i].startswith("0x"):
            frames = names.setdefault(f"{int(out[i], 16):08x}", [])
            i += 1
        else:
            frames.append((out[i], short(out[i + 1])))
            i += 2
    return names


def main() -> None:
    log, elf = sys.argv[1], sys.argv[2]
    addr2line = sys.argv[3] if len(sys.argv) > 3 else find_addr2line(elf)
    text = Path(log).read_text(encoding="utf-8", errors="replace")
    entries = [
        (idx, int(ms), int(size), pcs.split())
        for idx, ms, size, pcs in ENTRY.findall(text)
    ]
    wanted = {
        pc for _, _, size, pcs in entries if size for pc in pcs if pc != "00000000"
    }
    names = decode(addr2line, elf, sorted(wanted)) if wanted else {}
    for idx, ms, size, pcs in entries:
        if not size:
            print(
                f"--- marker at {ms / 1000:.3f} s: {int(pcs[0], 16)} {int(pcs[1], 16)}"
            )
            continue
        print(f"#{idx} {ms / 1000:.3f} s {size} B")
        for pc in pcs:
            if pc == "00000000":
                break
            for func, loc in names[pc]:
                print(f"    {func[:110]}  {loc}")


if __name__ == "__main__":
    main()
