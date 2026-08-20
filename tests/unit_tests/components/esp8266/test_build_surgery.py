"""Tests for the linker-script surgery shared with the native toolchain."""

from __future__ import annotations

import pytest

from esphome.components.esp8266.boards import BOARDS, ESP8266_BOARD_BUILD
from esphome.components.esp8266.build_surgery import (
    RATETABLE_RULE,
    apply_testing_memory_patches,
    relocate_ratetable,
    segment_length,
)

_COMMON_LD_SNIPPET = """\
  .dport0.data : ALIGN(4)
  {
    _dport0_data_start = ABSOLUTE(.);
  } >dport0_0_seg :dport0_0_phdr
  .data : ALIGN(4)
  {
    _data_start = ABSOLUTE(.);
    *(.data)
  } >dram0_0_seg :dram0_0_phdr
"""

_FLASH_LD_SNIPPET = """\
MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  iram1_0_seg :                         org = 0x40100000, len = 0x8000
  irom0_0_seg :                         org = 0x40201010, len = 0xfeff0
}
"""


def test_relocate_ratetable_inserts_after_data_start() -> None:
    patched = relocate_ratetable(_COMMON_LD_SNIPPET)
    assert RATETABLE_RULE in patched
    # Inserted after the .data section's anchor, not the .dport0.data one
    assert patched.index("_data_start = ABSOLUTE(.);") < patched.index(RATETABLE_RULE)
    assert patched.index(RATETABLE_RULE) < patched.index("*(.data)")
    # Idempotent on an already-patched script
    assert relocate_ratetable(patched) == patched


def test_relocate_ratetable_requires_anchor() -> None:
    with pytest.raises(RuntimeError, match="_data_start"):
        relocate_ratetable("SECTIONS { }")


def test_testing_memory_patches_enlarge_segments() -> None:
    patched = apply_testing_memory_patches(
        _FLASH_LD_SNIPPET, ("iram1_0_seg", "dram0_0_seg", "irom0_0_seg")
    )
    assert segment_length(patched, "iram1_0_seg") == 0x200000
    assert segment_length(patched, "dram0_0_seg") == 0x200000
    assert segment_length(patched, "irom0_0_seg") == 0x2000000
    # Untouched segments keep their sizes
    assert segment_length(patched, "dport0_0_seg") == 0x10


def test_segment_length() -> None:
    assert segment_length(_FLASH_LD_SNIPPET, "irom0_0_seg") == 0xFEFF0
    assert segment_length(_FLASH_LD_SNIPPET, "missing_seg") is None


def test_testing_memory_patches_missing_segment_raises() -> None:
    """A named segment the patch could not find raises instead of silently
    keeping the real memory limits."""
    with pytest.raises(RuntimeError, match="dram0_0_seg"):
        apply_testing_memory_patches("MEMORY { }", ("dram0_0_seg",))


def test_board_build_covers_every_board() -> None:
    """Every supported board has native build metadata (the table may carry
    extras that BOARDS does not expose)."""
    assert set(BOARDS) <= set(ESP8266_BOARD_BUILD)


def test_surgery_fingerprint_tracks_inputs() -> None:
    """The fingerprint changes with any behavioral input, so linker-script
    caches stamped with it self-invalidate on surgery edits."""
    from unittest.mock import patch

    from esphome.components.esp8266 import build_surgery

    base = build_surgery.surgery_fingerprint()
    assert base == build_surgery.surgery_fingerprint()
    with patch.object(build_surgery, "_TESTING_SEGMENT_SIZES", {"iram1_0_seg": "0x1"}):
        assert build_surgery.surgery_fingerprint() != base
