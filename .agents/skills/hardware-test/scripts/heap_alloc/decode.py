"""Decode an allocring dump into callers: decode.py LOG ELF [ADDR2LINE]

Prints each entry with its caller frames as function and file:line, and markers (size 0xFFFFFFFF, ALLOC_MARKER) as phase
separators.
ADDR2LINE defaults to an Xtensa one found in ESPHome's ESP-IDF tools cache, ~/.platformio or ~/.espressif.
Exits 2 when the dump is incomplete (missing header or entries, or the buffer filled during the run).
"""

from pathlib import Path
import re
import subprocess
import sys

ENTRY = re.compile(r"\bE (\d+) (\d+) (\d+) ((?:[0-9a-f]{8} ?){5})")
HEADER = re.compile(
    r"\bDUMP n=(\d+) claimed=(\d+) total=\d+ armed_ms=(\d+) busy=(\d+) buf=([0-9a-f]{8})"
)
MARKER = 0xFFFFFFFF
XTENSA = 94  # ELF e_machine; the recorder walks frames only on Xtensa


def tool_roots() -> list[Path]:
    roots = [Path.home() / ".platformio", Path.home() / ".espressif"]
    try:
        from esphome.espidf.framework import get_idf_tools_path
    except ImportError:
        return roots
    return [get_idf_tools_path(), *roots]


def is_xtensa(elf: str) -> bool:
    return int.from_bytes(Path(elf).read_bytes()[18:20], "little") == XTENSA


def find_addr2line() -> str:
    for root in tool_roots():
        for path in root.glob("**/bin/xtensa-esp*-elf-addr2line"):
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


def last_dump(text: str) -> str:
    """The newest dump in the file (a retried press or an appended file holds several); check() rejects it if cut off."""
    start = max((m.start() for m in HEADER.finditer(text)), default=len(text))
    return text[start:]


def check(dump: str, entries: list, xtensa: bool) -> list[str]:
    """Reasons the dump is incomplete, empty when it is whole."""
    if not (header := HEADER.search(dump)):
        return ["no DUMP header (log subscription started late?)"]
    n, claimed, armed_ms, busy = (int(header[i]) for i in range(1, 5))
    problems = []
    if not armed_ms:
        problems.append("the recorder was never armed (pressed before alloc_arm_s?)")
    if busy:
        problems.append(f"{busy} recorders were still writing an entry")
    indexes = [int(e[0]) for e in entries]
    if missing := sorted(set(range(n)) - set(indexes)):
        problems.append(f"{len(missing)} of {n} entries missing, first {missing[:10]}")
    if len(indexes) != len(set(indexes)):
        problems.append("duplicate entries")
    if stray := sorted(i for i in set(indexes) if i >= n):
        problems.append(f"entries past n={n}: {stray[:10]}")
    allocs = [pcs for _, _, size, pcs in entries if size != MARKER]
    if xtensa and allocs and all(pcs[0] == "00000000" for pcs in allocs):
        problems.append("no entry has a caller (frame walk failed?)")
    if claimed > n:
        problems.append(f"buffer filled: recording stopped after {n} entries")
    if "DUMP END" not in dump:
        problems.append("no DUMP END")
    return problems


def main() -> None:
    log, elf = sys.argv[1], sys.argv[2]
    text = last_dump(Path(log).read_text(encoding="utf-8", errors="replace"))
    entries = [
        (idx, int(ms), int(size), pcs.split())
        for idx, ms, size, pcs in ENTRY.findall(text)
    ]
    wanted = {
        pc
        for _, _, size, pcs in entries
        if size != MARKER
        for pc in pcs
        if pc != "00000000"
    }
    # Only Xtensa dumps carry callers; RISC-V ones still print sizes and times
    names = {}
    if wanted:
        addr2line = sys.argv[3] if len(sys.argv) > 3 else find_addr2line()
        names = decode(addr2line, elf, sorted(wanted))
    for idx, ms, size, pcs in entries:
        if size == MARKER:
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
    problems = check(text, entries, is_xtensa(elf))
    # addr2line names any address, so check the ELF is the build that made the dump
    if wanted and (header := HEADER.search(text)):
        nm = addr2line[: -len("addr2line")] + "nm"
        if not Path(nm).is_file():
            problems.append(f"cannot check the ELF: no {nm} next to addr2line")
        elif not re.search(
            rf"^0*{int(header[5], 16):x} [bBdD] g_allocs$",
            subprocess.run(
                [nm, elf], capture_output=True, text=True, check=True
            ).stdout,
            re.MULTILINE,
        ):
            problems.append(
                "the ELF is not the build that made this dump (g_allocs address differs)"
            )
    if problems:
        print("INCOMPLETE DUMP: " + "; ".join(problems), file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
