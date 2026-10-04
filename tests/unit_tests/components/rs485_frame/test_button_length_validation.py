"""Tests for rs485_frame button frame-length validation against the hub's
max_frame_length:.

A code review of the upstream PR found that a button whose resolved on-wire frame
(frame_type: + payload:, or command_format's preamble + encoded value(s) + postamble)
exceeds the hub's max_frame_length: is only ever caught at runtime, by
queue_raw_frame()/queue_command_values() dropping the frame with a warning log line every
single time the button is pressed. A button that can never transmit should be rejected at
config-validate time instead.
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
