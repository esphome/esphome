"""Tests for DriverChip.get_sequence's reset-delay handling."""

import pytest

from esphome.components.mipi import CONF_INVERT_COLORS, CONF_PIXEL_MODE, DriverChip

# A minimal config with no reset pin: enough for get_sequence(add_madctl=False) to run
# without needing a full display configuration.
_BASE_CONFIG = {CONF_PIXEL_MODE: "16bit", CONF_INVERT_COLORS: False}


def test_get_sequence_defaults_to_10ms_reset_delay() -> None:
    """A model with no reset_delay default falls back to a 10ms settling delay."""
    chip = DriverChip("TEST-GET-SEQUENCE-DEFAULT")

    sequence = chip.get_sequence(_BASE_CONFIG, add_madctl=False, add_reset=True)

    # SWRESET ({1, 0}) is prepended (no reset pin configured), followed by the
    # 10ms settling delay, flattened to {10, 255}.
    assert sequence[:4] == (1, 0, 10, 255)


def test_get_sequence_uses_model_reset_delay_default() -> None:
    """A model's own reset_delay default overrides the base 10ms default."""
    chip = DriverChip("TEST-GET-SEQUENCE-CUSTOM-DELAY", reset_delay=99)

    sequence = chip.get_sequence(_BASE_CONFIG, add_madctl=False, add_reset=True)

    assert sequence[:4] == (1, 0, 99, 255)


@pytest.mark.parametrize("reset_delay", [-1, 255])
def test_get_sequence_rejects_out_of_range_reset_delay(reset_delay: int) -> None:
    """reset_delay outside 0-254ms is rejected."""
    chip = DriverChip("TEST-GET-SEQUENCE-BAD-DELAY", reset_delay=reset_delay)

    with pytest.raises(ValueError, match="reset_delay must be between 0 and 254ms"):
        chip.get_sequence(_BASE_CONFIG, add_madctl=False, add_reset=True)
