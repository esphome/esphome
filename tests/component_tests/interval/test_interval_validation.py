"""Tests for the interval config validation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.interval import _MAX_PERIOD_MS, _uint32_milliseconds
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE


@pytest.mark.parametrize(
    ("value", "milliseconds"),
    [("0s", 0), ("1s", 1000), (f"{_MAX_PERIOD_MS}ms", _MAX_PERIOD_MS)],
)
def test_period_up_to_the_max_is_accepted(value: str, milliseconds: int) -> None:
    assert _uint32_milliseconds()(value).total_milliseconds == milliseconds


def test_period_past_the_max_is_rejected() -> None:
    """UINT32_MAX would mean never run; past it the value would be cut down on the device."""
    with pytest.raises(cv.Invalid):
        _uint32_milliseconds()(f"{_MAX_PERIOD_MS + 1}ms")


def test_config_with_startup_delay_past_the_max_is_invalid(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("interval_long_delay.yaml")
    assert read_config({}) is None
    # Rejected for the bound (only startup_delay exceeds it), not an unrelated fixture error
    assert f"value must be at most {_MAX_PERIOD_MS}ms." in capsys.readouterr().out
