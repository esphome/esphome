"""compare_spi.py BASE PR: compare two SX127x SPI record files (sx127x_spi_record_test.cpp or the device sim).

Each file is a host record file or a device serial log with sxrec.hpp's "REC ..." lines (the log prefix is cut at
"REC " and "REC +" continuation lines are joined).

Register files and FIFO bytes must match exactly. Each transaction is flattened into per-register operations with
burst auto-increment (the FIFO address 0x00 does not move); the flattened streams must match, so the raw streams can
differ only in how the operations are grouped into chip select windows.
"""

from collections import Counter
import pathlib
import re
import sys

ANSI = re.compile(r"\x1b\[[0-9;]*m")


def lines(path: str) -> list[str]:
    """The record lines of a host record file, or only the "REC" lines of a device log."""
    plain: list[str] = []
    rec: list[str] = []
    with pathlib.Path(path).open(encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = ANSI.sub("", raw).strip()
            if (i := line.find("REC ")) < 0:
                plain.append(line)
            elif (body := line[i + 4 :]).startswith("+"):
                if rec:
                    rec[-1] += body[1:]
            else:
                rec.append(body)
    return rec or plain


def parse(path: str) -> dict[str, dict]:
    cases: dict[str, dict] = {}
    cur = None
    for line in lines(path):
        if line.startswith("CASE "):
            if line[5:] in cases:
                sys.exit(f"{path}: case {line[5:]!r} appears twice")
            cur = {"T": [], "REGS": None, "FIFO": None, "END": False}
            cases[line[5:]] = cur
        elif cur is None:
            continue
        elif line.startswith("T"):
            cur["T"].append([int(x, 16) for x in line[1:].split()])
        elif line.startswith("REGS"):
            cur["REGS"] = line[4:].strip()
        elif line.startswith("FIFO"):
            cur["FIFO"] = line[4:].strip()
        elif line == "END":
            cur["END"] = True
    if not cases:
        sys.exit(f"{path}: no CASE records")
    if partial := [
        n
        for n, c in cases.items()
        if c["REGS"] is None or c["FIFO"] is None or not c["END"]
    ]:
        sys.exit(
            f"{path}: {len(partial)} cases without REGS, FIFO or END, first {partial[0]!r}"
        )
    return cases


def flatten(transactions: list[list[int]]) -> list[tuple]:
    ops = []
    for t in transactions:
        if not t:
            continue
        addr, write = t[0] & 0x7F, bool(t[0] & 0x80)
        for value in t[1:]:
            ops.append(("W" if write else "R", addr, value if write else None))
            if addr != 0:
                addr = (addr + 1) & 0x7F
    return ops


def burst_shape(transactions: list[list[int]]) -> Counter:
    """Writes of more than one byte to normal registers, by start address and length."""
    return Counter(
        (t[0] & 0x7F, len(t) - 1)
        for t in transactions
        if t and t[0] & 0x80 and t[0] & 0x7F != 0 and len(t) > 2
    )


def main() -> None:
    base, pr = parse(sys.argv[1]), parse(sys.argv[2])
    if base.keys() != pr.keys():
        print("only in base:", sorted(base.keys() - pr.keys()))
        print("only in pr:", sorted(pr.keys() - base.keys()))
        sys.exit("different case lists")
    bad = 0
    raw_same = 0
    tb = tp = 0
    shapes: Counter = Counter()
    fifo_tx = Counter()
    for name in base:
        b, p = base[name], pr[name]
        if b["REGS"] != p["REGS"] or b["FIFO"] != p["FIFO"]:
            bad += 1
            print("REGISTER/FIFO MISMATCH", name)
        if flatten(b["T"]) != flatten(p["T"]):
            bad += 1
            print("FLATTENED MISMATCH", name)
        raw_same += b["T"] == p["T"]
        tb += len(b["T"])
        tp += len(p["T"])
        shapes += burst_shape(p["T"])
        fifo_tx[
            (
                sum(1 for t in b["T"] if t and t[0] == 0x80),
                sum(1 for t in p["T"] if t and t[0] == 0x80),
            )
        ] += 1
    print(f"cases={len(base)} mismatches={bad} raw_identical={raw_same}")
    print(f"transactions base={tb} pr={tp}")
    print("PR multi-byte register bursts (start, bytes): count", dict(shapes))
    print("FIFO write transactions per case (base, pr): count", dict(fifo_tx))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
