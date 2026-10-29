"""Tests for map_sequence's delay parsing."""

import pytest

from esphome import config_validation as cv
from esphome.components.mipi import DELAY_FLAG, map_sequence


def test_map_sequence_delay_with_unit() -> None:
    """The documented 'delay Nms' form keeps working."""
    assert map_sequence("delay 5ms") == (DELAY_FLAG, 5)


def test_map_sequence_delay_without_unit_is_milliseconds() -> None:
    """A bare 'delay N' is milliseconds, as the docstring and the get_sequence
    reset_delay comment both describe."""
    assert map_sequence("delay 5") == (DELAY_FLAG, 5)


def test_map_sequence_delay_out_of_range_rejected() -> None:
    """Both forms respect the 1-255ms range."""
    for form in ("delay 0", "delay 256", "delay 0ms", "delay 256ms"):
        with pytest.raises(cv.Invalid):
            map_sequence(form)


def test_map_sequence_delay_invalid_value_rejected() -> None:
    """A non-numeric delay is still rejected."""
    with pytest.raises(cv.Invalid):
        map_sequence("delay abc")


def test_map_sequence_command_entry() -> None:
    """Command entries are unaffected by the delay handling."""
    assert map_sequence([0x01]) == (0x01,)
    assert map_sequence([0xFF, 0xAA, 0xBB]) == (0xFF, 0xAA, 0xBB)
