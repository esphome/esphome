"""Tests for qspi_dbi map_sequence's delay parsing."""

import pytest

from esphome import config_validation as cv
from esphome.components.qspi_dbi.display import DELAY_FLAG, map_sequence
from esphome.core import TimePeriod


def test_map_sequence_delay_with_unit() -> None:
    """The documented 'delay Nms' form keeps working."""
    assert map_sequence("delay 5ms") == [TimePeriod(milliseconds=5), DELAY_FLAG]


def test_map_sequence_delay_without_unit_is_milliseconds() -> None:
    """A bare 'delay N' is milliseconds, as the docstring describes."""
    assert map_sequence("delay 5") == [TimePeriod(milliseconds=5), DELAY_FLAG]


@pytest.mark.parametrize("form", ["delay 0", "delay 256", "delay 0ms", "delay 256ms"])
def test_map_sequence_delay_out_of_range_rejected(form: str) -> None:
    """Both forms respect the 1-255ms range."""
    with pytest.raises(cv.Invalid):
        map_sequence(form)


def test_map_sequence_command_entry() -> None:
    """Command entries are unaffected by the delay handling."""
    assert map_sequence([0x01]) == [0x01, 0]
    assert map_sequence([0xFF, 0xAA, 0xBB]) == [0xFF, 2, 0xAA, 0xBB]
