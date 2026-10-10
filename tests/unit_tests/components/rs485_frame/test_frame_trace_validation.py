"""Tests for rs485_frame frame_trace: schema validation and the
rs485_frame.dump_frame_trace action's cross-check against its target hub."""

from __future__ import annotations

from pathlib import Path

import voluptuous as vol

from esphome import config as esphome_config, yaml_util
from esphome.core import CORE


def test_frame_trace_with_non_default_values_validates(fixture_path: Path) -> None:
    """size/capture_bytes/include_invalid all set to non-default values, plus
    dump_frame_trace wired against the same hub, must validate cleanly."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(fixture_path / "rs485_frame_frame_trace_ok.yaml")

    result = esphome_config.validate_config(raw_config, {})

    assert not result.errors, f"expected no validation errors, got: {result.errors}"


def test_frame_trace_size_above_upper_bound_raises_clean_invalid(
    fixture_path: Path,
) -> None:
    """frame_trace.size above FRAME_TRACE_SIZE_UPPER (128) must be rejected with a
    clear cv.Invalid, not silently truncated -- the C++ FixedVector capacity is capped
    at the same constant, so a larger schema value would otherwise be misleading."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_frame_trace_size_too_large.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert result.errors, "expected a validation error for size: above the upper bound"
    assert all(isinstance(err, vol.Invalid) for err in result.errors)


def test_frame_trace_capture_bytes_above_upper_bound_raises_clean_invalid(
    fixture_path: Path,
) -> None:
    """frame_trace.capture_bytes above FRAME_TRACE_CAPTURE_BYTES_UPPER (255) must be
    rejected -- FrameTraceEntry::captured_len is uint8_t-backed, so 256 would wrap to 0."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_frame_trace_capture_bytes_too_large.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert result.errors, (
        "expected a validation error for capture_bytes: above the upper bound"
    )
    assert all(isinstance(err, vol.Invalid) for err in result.errors)


def test_dump_frame_trace_action_against_hub_with_no_frame_trace_block_raises_clean_invalid(
    fixture_path: Path,
) -> None:
    """rs485_frame.dump_frame_trace referencing a hub with no frame_trace: block must be
    rejected at validation time, not silently compile into a no-op action."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_dump_frame_trace_missing_block.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert result.errors, (
        "expected a validation error for dump_frame_trace: against a hub with no "
        "frame_trace: block"
    )
    assert all(isinstance(err, vol.Invalid) for err in result.errors)
    assert any("frame_trace" in str(err) for err in result.errors)
