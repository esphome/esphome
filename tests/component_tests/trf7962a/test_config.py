"""Tests for the trf7962a config validation."""

from __future__ import annotations

import pytest

from esphome.components.trf7962a import _validate_data_rate
import esphome.config_validation as cv


def test_data_rate_up_to_the_datasheet_limit_is_accepted() -> None:
    assert _validate_data_rate(10e6) == 10e6


def test_data_rate_above_the_datasheet_limit_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="must not exceed 10MHz"):
        _validate_data_rate(20e6)
