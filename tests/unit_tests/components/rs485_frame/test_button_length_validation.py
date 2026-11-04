"""Tests for rs485_frame frame-length validation of buttons and send_frame.

A button whose resolved frame (frame_type: + payload:, or command_format's preamble +
encoded value(s) + postamble) is longer than the hub's max_frame_length: would otherwise
only be caught at runtime, by check_tx_() dropping it with a warning on every press. A
frame under 2 bytes is shorter than rs485_frame accepts on receive, so it is not sent
either. Both are rejected at config-validate time instead.
"""

from __future__ import annotations

from pathlib import Path

import voluptuous as vol

from esphome import config as esphome_config, yaml_util
from esphome.core import CORE


def test_button_raw_frame_exceeding_max_frame_length_raises_clean_invalid(
    fixture_path: Path,
) -> None:
    """A raw frame_type: + payload: button whose combined length exceeds the hub's
    max_frame_length: must be rejected."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_button_raw_exceeds_max_frame_length.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert result.errors, (
        "expected a validation error for a raw button exceeding max_frame_length"
    )
    assert all(isinstance(err, vol.Invalid) for err in result.errors)
    assert any("max_frame_length" in str(err) for err in result.errors)


def test_button_value_encoding_exceeding_max_frame_length_raises_clean_invalid(
    fixture_path: Path,
) -> None:
    """A value: button whose command_format-encoded frame (preamble + values +
    postamble) exceeds the hub's max_frame_length: must be rejected."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_button_value_exceeds_max_frame_length.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert result.errors, (
        "expected a validation error for a value: button exceeding max_frame_length"
    )
    assert all(isinstance(err, vol.Invalid) for err in result.errors)
    assert any("max_frame_length" in str(err) for err in result.errors)


def test_button_frame_under_two_bytes_raises_clean_invalid(fixture_path: Path) -> None:
    """rs485_frame drops received frames under 2 bytes (before the CRC), so a button
    that resolves to a shorter frame must be rejected."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_button_frame_too_short.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert any("at least 2 bytes" in str(err) for err in result.errors), [
        str(err) for err in result.errors
    ]


def test_send_frame_constant_under_two_bytes_raises_clean_invalid(
    fixture_path: Path,
) -> None:
    """send_frame with constant frame_type and payload totalling under 2 bytes must be
    rejected, for the same reason as the button."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_send_frame_too_short.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert any("at least 2 bytes" in str(err) for err in result.errors), [
        str(err) for err in result.errors
    ]
