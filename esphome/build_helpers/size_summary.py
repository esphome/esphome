"""The PlatformIO-format size bar shared by the native toolchains."""

from __future__ import annotations


def format_bar(used: int, total: int) -> str:
    """Match PlatformIO's ``_format_availale_bytes`` (pioupload.py) exactly.

    The upstream helper's name really is spelled that way; keep the citation
    verbatim so it stays greppable in the PlatformIO source.
    """
    pct_raw = used / total if total else 0
    blocks = 10
    filled = min(int(round(blocks * pct_raw)), blocks)
    progress = "=" * filled
    return (
        f"[{progress:<{blocks}}] {pct_raw: 6.1%} "
        f"(used {used:d} bytes from {total:d} bytes)"
    )
