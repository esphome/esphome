"""Linker-script surgery shared with the native (PlatformIO-free) toolchain.

These mirror the PlatformIO extra scripts in this directory
(``relocate_ratetable.py.script`` and ``testing_mode.py.script``), which run
inside SCons and must stay self-contained. The native build generator applies
the same patches to the linker scripts it generates, so the logic lives here
as plain functions. Keep both in sync when changing either.
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


_TESTING_SEGMENT_SIZES = (
    ("iram1_0_seg", TESTING_IRAM_SIZE),
    ("dram0_0_seg", TESTING_DRAM_SIZE),
    ("irom0_0_seg", TESTING_FLASH_SIZE),
)


def _patch_segment_size(content: str, segment_name: str, new_size: str) -> str:
    pattern = (
        rf"({segment_name}\s*:\s*org\s*=\s*0x[0-9a-fA-F]+\s*,\s*len\s*=\s*)"
        r"0x[0-9a-fA-F]+"
    )
    return re.sub(pattern, rf"\g<1>{new_size}", content)


def apply_testing_memory_patches(content: str, require: Collection[str]) -> str:
    """Enlarge IRAM/DRAM/flash segments so grouped CI test builds can link.

    ``require`` names the segments this file must define; a silently
    unpatched segment would keep the real memory limits and fail grouped
    builds far from the cause. The segments are split across the two linker
    scripts (iram1_0_seg in the generated common one, dram0_0_seg and
    irom0_0_seg in the flash one), so each caller requires only its own.
    """
    missing = set(require)
    for segment, size in _TESTING_SEGMENT_SIZES:
        patched = _patch_segment_size(content, segment, size)
        if patched != content:
            missing.discard(segment)
        content = patched
    if missing:
        raise RuntimeError(
            f"Testing-mode memory patch failed: segment(s) {', '.join(sorted(missing))} "
            "not found (has the Arduino core linker script changed?)"
        )
    return content


def segment_length(content: str, segment_name: str) -> int | None:
    """Read a memory segment's length from linker script content."""
    match = re.search(
        rf"{segment_name}\s*:.+len\s*=\s*(0x[\da-fA-F]+)",
        content,
    )
    return int(match.group(1), 16) if match else None
