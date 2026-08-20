"""Linker-script surgery shared with the native (PlatformIO-free) toolchain.

These mirror the PlatformIO extra scripts in this directory
(``relocate_ratetable.py.script`` and ``testing_mode.py.script``), which run
inside SCons and must stay self-contained. The native build generator applies
the same patches to the linker scripts it generates, so the logic lives here
as plain functions. Keep both in sync when changing either.
"""

from __future__ import annotations

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


def _patch_segment_size(content: str, segment_name: str, new_size: str) -> str:
    pattern = (
        rf"({segment_name}\s*:\s*org\s*=\s*0x[0-9a-fA-F]+\s*,\s*len\s*=\s*)"
        r"0x[0-9a-fA-F]+"
    )
    return re.sub(pattern, rf"\g<1>{new_size}", content)


def apply_testing_memory_patches(content: str) -> str:
    """Enlarge IRAM/DRAM/flash segments so grouped CI test builds can link."""
    content = _patch_segment_size(content, "iram1_0_seg", TESTING_IRAM_SIZE)
    content = _patch_segment_size(content, "dram0_0_seg", TESTING_DRAM_SIZE)
    return _patch_segment_size(content, "irom0_0_seg", TESTING_FLASH_SIZE)
