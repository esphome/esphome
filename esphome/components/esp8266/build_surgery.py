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
import hashlib
import re

# Move the NONOS SDK wifi rate tables from flash to DRAM; see
# relocate_ratetable.py.script for the full background (NONOS SDK issue 320).
RATETABLE_RULE = "*libnet80211.a:ieee80211_phy.o(.irom.text .irom.text.*)"
_RATETABLE_COMMENT = (
    "/* ESPHome: wifi rate tables must live in DRAM, see NONOS SDK issue 320 */"
)
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
        + f"\n    {_RATETABLE_COMMENT}"
        + f"\n    {RATETABLE_RULE}"
        + content[insert_pos:]
    )


_TESTING_SEGMENT_SIZES = {
    "iram1_0_seg": TESTING_IRAM_SIZE,
    "dram0_0_seg": TESTING_DRAM_SIZE,
    "irom0_0_seg": TESTING_FLASH_SIZE,
}


def _segment_line_re(segment_name: str) -> re.Pattern[str]:
    """The MEMORY line for one segment: ``<seg> : org = 0x..., len = 0x...``.

    Anchored to the start of the line so a name never matches inside a
    longer one (``ram0_0_seg`` must not read ``dram0_0_seg``). The size
    group stops at the hex digits, leaving any ``ul`` suffix (from the
    preprocessed ``MMU_IRAM_SIZE``) in place.
    """
    return re.compile(
        rf"(^[ \t]*{re.escape(segment_name)}"
        r"\s*:\s*org\s*=\s*0x[0-9a-fA-F]+\s*,\s*len\s*=\s*)"
        r"(0x[0-9a-fA-F]+)",
        re.MULTILINE,
    )


def apply_testing_memory_patches(content: str, segments: Collection[str]) -> str:
    """Enlarge the named memory segments so grouped CI test builds can link.

    Each caller passes the segments its linker script defines: the
    generated common ld carries ``iram1_0_seg``; the flash ld carries
    ``dram0_0_seg`` and ``irom0_0_seg``. A segment that fails to match
    raises, since a silently kept real memory limit would fail grouped
    builds far from the cause.
    """
    for segment in _TESTING_SEGMENT_SIZES:
        if segment not in segments and _segment_line_re(segment).search(content):
            raise RuntimeError(
                f"Testing-mode segment {segment} is present in the linker "
                "script but was not selected for patching"
            )
    for segment in segments:
        if segment not in _TESTING_SEGMENT_SIZES:
            raise RuntimeError(f"Unknown testing-mode segment {segment!r}")
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
    """Read a memory segment's length from linker script content.

    Returns None for an absent segment OR an unparsable line; callers must
    treat None as "no usable budget" and warn (as the Flash summary does),
    never as "no limit".
    """
    match = _segment_line_re(segment_name).search(content)
    return int(match.group(2), 16) if match else None


def surgery_fingerprint() -> str:
    """Hash of this module's source; linker-script caches include it so an
    edit here invalidates them."""
    import inspect
    import sys

    source = inspect.getsource(sys.modules[__name__])
    return hashlib.sha256(source.encode()).hexdigest()
