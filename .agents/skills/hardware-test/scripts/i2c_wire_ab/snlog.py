"""Readers for the sweep lines of dev/snf.hpp (SN) and ard/swp.hpp (SW), shared by the comparers."""

from collections.abc import Mapping
import pathlib
import re

ANSI = re.compile(r"\x1b\[[0-9;]*m")
FIELDS = r"\|rc=(-?\d+)\|alloc=(\d+)\|ev=\d+\|amb=\d+\|ovf=\d+\|fnv=([0-9A-F]+)\|(.*)$"
CAPTURES = ("", "2", "3")


def load(path: str | pathlib.Path, tag: str = "SN") -> dict[str, dict]:
    """Case label -> rc, alloc, fnv and the rest of the line.

    SN lines get `verdict` (OK or DIFF) and, on a DIFF, the `got` and `exp` token lists of the SNGOT and SNEXP lines
    that follow. SW lines get `wire` (the decoded stream) and `rd` (read data).
    """
    line_re = re.compile(rf"{tag} (.+?){FIELDS}")
    cases: dict[str, dict] = {}
    cur: dict | None = None
    text = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    for line in text.splitlines():
        line = ANSI.sub("", line).rstrip()
        if m := line_re.search(line):
            label, rc, alloc, fnv, rest = m.groups()
            cur = cases[label] = {"rc": int(rc), "alloc": int(alloc), "fnv": fnv}
            if tag == "SW":
                wire, _, rd = rest.partition("|rd=")
                cur.update(wire=wire.strip(), rd=rd.strip())
            else:
                cur["verdict"] = rest
        elif cur is not None and (m := re.search(r"SN(GOT|EXP) (.*)$", line)):
            cur["got" if m.group(1) == "GOT" else "exp"] = m.group(2).split()
    return cases


def is_read(label: str) -> bool:
    """Reads return chip data, so their wire hash changes between runs."""
    return label.startswith(("rreg", "read", "write1+read"))


def nack_cut(got: list[str], exp: list[str]) -> int | None:
    """Index of the token the chip NACKed when `got` is `exp` cut there (then STOP), else None.

    Only address and write tokens count as a chip NACK; a read token ("r..") NACKed early stays a difference.
    """
    j = next(
        (j for j, t in enumerate(got) if t.endswith("-") and not t.startswith("r")),
        None,
    )
    if (
        j is not None
        and got[:j] == exp[:j]
        and got[j][:-1] == exp[j][:-1]
        and got[j + 1 :] == ["P"]
    ):
        return j
    return None


def runs(name: str) -> list[dict[str, dict]]:
    """The captures NAME-ser.txt, NAME-ser2.txt and NAME-ser3.txt that exist (at least one)."""
    found = [pathlib.Path(f"{name}-ser{s}.txt") for s in CAPTURES]
    loaded = [load(p) for p in found if p.exists()]
    if not loaded:
        raise SystemExit(f"no capture {name}-ser.txt")
    return loaded


def first_alloc(allocs: Mapping[str, int], shape: str, limit: int = 400) -> int | None:
    """First length of `shape` (wreg8, wreg16) whose call allocated; allocs maps case label -> malloc count."""
    return next(
        (n for n in range(limit) if allocs.get(f"{shape} len={n}", 0) > 0), None
    )
