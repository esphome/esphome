"""Linker-script surgery shared with the native (PlatformIO-free) toolchain.

These mirror the PlatformIO extra scripts in this directory
(``relocate_ratetable.py.script`` and ``testing_mode.py.script``), which run
inside SCons and must stay self-contained. The native build generator applies
the same patches to the linker scripts it generates, so the logic lives here
as plain functions. Keep both in sync when changing either.
``segment_length`` is native-toolchain-only and has no script twin.
"""

from __future__ import annotations

from collections.abc import Collection
import re

# Move the NONOS SDK wifi rate tables from flash to DRAM; see
# relocate_ratetable.py.script for the full background (NONOS SDK issue 320).
RATETABLE_RULE = "*libnet80211.a:ieee80211_phy.o(.irom.text .irom.text.*)"
# Match the whole line: "_data_start" is also a substring of the
# "_dport0_data_start" line in the earlier .dport0.data section
_RATETABLE_ANCHOR = re.compile(r"^\s*_data_start = ABSOLUTE\(\.\);", re.MULTILINE)

# Memory sizes for testing mode (allow larger builds for CI component grouping)
TESTING_IRAM_SIZE = "0x200000"  # 2MB
TESTING_DRAM_SIZE = "0x200000"  # 2MB
TESTING_FLASH_SIZE = "0x2000000"  # 32MB


def relocate_ratetable(content: str) -> str:
    """Insert the rate-table DRAM rule into a generated common linker script."""
    if RATETABLE_RULE in content:
        return content
    match = _RATETABLE_ANCHOR.search(content)
    if match is None:
        raise RuntimeError(
            "'_data_start' anchor not found in the generated linker script; "
            "cannot apply wifi rate table DRAM relocation "
            "(has the Arduino core linker script changed?)"
        )
    insert_pos = match.end()
    return (
        content[:insert_pos]
        + "\n    /* ESPHome: wifi rate tables must live in DRAM, see NONOS SDK issue 320 */"
        + f"\n    {RATETABLE_RULE}"
        + content[insert_pos:]
    )


_TESTING_SEGMENT_SIZES = {
    "iram1_0_seg": TESTING_IRAM_SIZE,
    "dram0_0_seg": TESTING_DRAM_SIZE,
    "irom0_0_seg": TESTING_FLASH_SIZE,
}


def _segment_line_re(segment_name: str) -> re.Pattern[str]:
    """The MEMORY line for one segment: ``<seg> : org = 0x..., len = 0x...``."""
    return re.compile(
        rf"({segment_name}\s*:\s*org\s*=\s*0x[0-9a-fA-F]+\s*,\s*len\s*=\s*)"
        r"(0x[0-9a-fA-F]+)"
    )


def apply_testing_memory_patches(content: str, segments: Collection[str]) -> str:
    """Enlarge the named memory segments so grouped CI test builds can link.

    Each caller passes the segments its linker script defines; a segment
    that fails to match raises, since a silently kept real memory limit
    would fail grouped builds far from the cause.
    """
    for segment in segments:
        content, count = _segment_line_re(segment).subn(
            rf"\g<1>{_TESTING_SEGMENT_SIZES[segment]}", content
        )
        if count == 0:
            raise RuntimeError(
                f"Testing-mode memory patch failed: segment {segment} "
                "not found (has the Arduino core linker script changed?)"
            )
    return content


def segment_length(content: str, segment_name: str) -> int | None:
    """Read a memory segment's length from linker script content."""
    match = _segment_line_re(segment_name).search(content)
    return int(match.group(2), 16) if match else None
