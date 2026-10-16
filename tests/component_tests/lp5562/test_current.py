"""Tests for the lp5562 output current option."""

import pytest

from esphome import config_validation as cv
from esphome.components.lp5562.output import _current_tenths


def test_current_is_stored_in_steps_of_0_1_ma() -> None:
    # The register counts in 0.1 mA: 17.5 mA is 175, the full scale 25.5 mA is 255
    assert _current_tenths(cv.current("17.5mA")) == 175
    assert _current_tenths(cv.current("25.5mA")) == 255
    assert _current_tenths(cv.current("0mA")) == 0


def test_current_between_two_steps_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="multiple of 0.1mA"):
        _current_tenths(cv.current("10.05mA"))


def test_current_above_25_5_ma_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="between 0mA and 25.5mA"):
        _current_tenths(cv.current("25.6mA"))
