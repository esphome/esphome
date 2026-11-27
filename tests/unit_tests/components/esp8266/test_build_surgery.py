"""Tests for the linker-script surgery shared with the native toolchain."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys

import pytest

from esphome.components.esp8266 import build_surgery
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

# Shaped like the real SDK flash ld scripts: no iram1_0_seg (that lives in
# the generated common ld only)
_FLASH_LD_SNIPPET = """\
MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  irom0_0_seg :                         org = 0x40201010, len = 0xfeff0
}
"""

# Shaped like the preprocessed common ld: MMU_IRAM_SIZE expands with a ul
# suffix the patcher must leave in place
_COMMON_LD_MEMORY_SNIPPET = """\
MEMORY
{
  iram1_0_seg :                         org = 0x40100000, len = 0x8000ul
}
"""


def test_relocate_ratetable_inserts_after_data_start() -> None:
    patched = relocate_ratetable(_COMMON_LD_SNIPPET)
    assert RATETABLE_RULE in patched
    # Inserted after the .data section's anchor, not the .dport0.data one
    # (whose closing brace bounds the decoy block)
    assert RATETABLE_RULE not in patched[: patched.index("} >dport0_0_seg")]
    assert patched.index(RATETABLE_RULE) < patched.index("*(.data)")
    # Idempotent on an already-patched script
    assert relocate_ratetable(patched) == patched


def test_relocate_ratetable_requires_anchor() -> None:
    with pytest.raises(RuntimeError, match="_data_start"):
        relocate_ratetable("SECTIONS { }")


def test_testing_memory_patches_enlarge_segments() -> None:
    patched = apply_testing_memory_patches(
        _FLASH_LD_SNIPPET, ("dram0_0_seg", "irom0_0_seg")
    )
    assert segment_length(patched, "dram0_0_seg") == 0x200000
    assert segment_length(patched, "irom0_0_seg") == 0x2000000
    # Untouched segments keep their sizes
    assert segment_length(patched, "dport0_0_seg") == 0x10


def test_testing_memory_patches_keep_ul_suffix() -> None:
    """The common ld's preprocessed sizes carry a ul suffix; the patch must
    replace only the hex digits, as testing_mode.py.script does."""
    patched = apply_testing_memory_patches(_COMMON_LD_MEMORY_SNIPPET, ("iram1_0_seg",))
    assert "len = 0x200000ul" in patched
    assert segment_length(patched, "iram1_0_seg") == 0x200000


def test_segment_length_requires_whole_name() -> None:
    """A name must match its own line, never inside a longer segment name."""
    assert segment_length(_FLASH_LD_SNIPPET, "ram0_0_seg") is None


def test_testing_memory_patches_unknown_segment_raises() -> None:
    with pytest.raises(RuntimeError, match="Unknown testing-mode segment"):
        apply_testing_memory_patches("MEMORY { }", ("bogus_seg",))


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


def test_surgery_fingerprint_is_stable_and_sensitive(tmp_path) -> None:
    """The properties the linker-script cache depends on: the fingerprint is
    stable across calls and changes when the module's source changes."""

    first = build_surgery.surgery_fingerprint()
    assert first == build_surgery.surgery_fingerprint()
    assert len(first) == 64
    int(first, 16)  # sha256 hex digest

    # A modified copy of the module must fingerprint differently
    copy = tmp_path / "build_surgery_variant.py"
    copy.write_text(
        Path(build_surgery.__file__).read_text(encoding="utf-8")
        + "\nEXTRA_BEHAVIORAL_INPUT = 1\n",
        encoding="utf-8",
    )
    spec = importlib.util.spec_from_file_location("build_surgery_variant", copy)
    variant = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = variant
    try:
        spec.loader.exec_module(variant)
        assert variant.surgery_fingerprint() != first
    finally:
        del sys.modules[spec.name]


def test_testing_memory_patches_present_but_unselected_raises() -> None:
    """A known segment left off the caller's list must fail, not silently
    keep its real memory limit."""
    with pytest.raises(RuntimeError, match="not selected"):
        apply_testing_memory_patches(_FLASH_LD_SNIPPET, ("dram0_0_seg",))
