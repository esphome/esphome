"""Tests for the tflite_model_loader build helpers."""

from pathlib import Path

import pytest

from esphome import cpp_types as ct
from esphome.components import tflite_model_loader
from esphome.const import CONF_RAW_DATA_ID
from esphome.core import CORE, ID


def _config() -> dict:
    return {CONF_RAW_DATA_ID: ID("model_data", is_declaration=True, type=ct.uint8)}


def test_the_model_buffer_is_aligned_for_the_flatbuffer_cast(setup_core: Path) -> None:
    """TFLM casts the buffer, so its address must be aligned like one."""
    CORE.main_statements = []

    tflite_model_loader.emit_model_buffer(_config(), b"\x01\x02")

    assert (
        str(CORE.main_statements[-1])
        == "alignas(16) static constexpr uint8_t model_data[] PROGMEM = {0x01, 0x02};"
    )


def test_the_model_buffer_falls_back_to_an_explicit_declaration(
    setup_core: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """ESPHome 2026.11 has no align argument; the fallback keeps the buffer aligned."""
    monkeypatch.setattr(tflite_model_loader, "_PROGMEM_ARRAY_SUPPORTS_ALIGN", False)
    CORE.global_statements = []

    tflite_model_loader.emit_model_buffer(_config(), b"\x01\x02")

    assert (
        str(CORE.global_statements[-1])
        == "alignas(16) static constexpr uint8_t model_data[] PROGMEM = {0x01, 0x02};"
    )
