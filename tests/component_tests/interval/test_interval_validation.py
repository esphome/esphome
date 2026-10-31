"""Tests for the interval config validation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.interval import _UINT32_MAX_MS, _uint32_milliseconds
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE


@pytest.mark.parametrize(
    ("value", "milliseconds"),
    [("0s", 0), ("1s", 1000), (f"{_UINT32_MAX_MS}ms", _UINT32_MAX_MS)],
)
def test_period_up_to_uint32_max_is_accepted(value: str, milliseconds: int) -> None:
    assert _uint32_milliseconds()(value).total_milliseconds == milliseconds


def test_period_past_uint32_max_is_rejected() -> None:
    """Past uint32_t the delay or interval would be silently cut down on the device."""
    with pytest.raises(cv.Invalid):
        _uint32_milliseconds()(f"{_UINT32_MAX_MS + 1}ms")


def test_config_with_startup_delay_past_uint32_max_is_invalid(
    component_config_path: Callable[[str], Path],
) -> None:
    CORE.config_path = component_config_path("interval_long_delay.yaml")
    assert read_config({}) is None
