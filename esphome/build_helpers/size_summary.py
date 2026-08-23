"""The PlatformIO-format size bar shared by the native toolchains."""

from __future__ import annotations


def format_bar(used: int, total: int) -> str:
    """Match PlatformIO's ``_format_availale_bytes`` (sic, pioupload.py) exactly."""
    if total <= 0:
        # A "from 0 bytes" bar is meaningless; make every caller handle it
        raise ValueError(f"non-positive size total {total}")
    pct_raw = used / total
    blocks = 10
    filled = min(int(round(blocks * pct_raw)), blocks)
    progress = "=" * filled
    return (
        f"[{progress:<{blocks}}] {pct_raw: 6.1%} "
        f"(used {used:d} bytes from {total:d} bytes)"
    )


def print_size_line(label: str, used: int, total: int) -> None:
    """One PlatformIO-format summary line (``RAM``/``Flash``).

    The label padding is part of the format: ``script/ci_memory_impact_extract.py``
    matches these lines verbatim.
    """
    print(f"{label + ':':<7}{format_bar(used, total)}")
