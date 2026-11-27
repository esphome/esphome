"""Tests for the ina226 alert register maths."""

import pytest

from esphome.components.ina226.sensor import (
    CONF_ALERT,
    CONF_FUNCTION,
    CONF_LIMIT,
    _alert_limit_register,
    _validate_alert,
)
import esphome.config_validation as cv
from esphome.const import CONF_MAX_CURRENT


def _config(function: str, limit: float, max_current: float = 3.2) -> dict:
    return {
        CONF_MAX_CURRENT: max_current,
        CONF_ALERT: {CONF_FUNCTION: function, CONF_LIMIT: limit},
    }


@pytest.mark.parametrize(
    ("function", "limit", "raw"),
    [
        ("shunt_over", 0.075, 30000),
        ("shunt_under", -0.01, -4000),
        ("bus_over", 15.0, 12000),
        # 3.2 A gives a 98 uA current LSB, so a power count is 2.45 mW
        ("power_over", 50.0, 20408),
    ],
)
def test_limit_register(function: str, limit: float, raw: int) -> None:
    assert _alert_limit_register(_config(function, limit)) == raw


@pytest.mark.parametrize(
    ("function", "limit"),
    [("shunt_over", 0.082), ("shunt_under", -0.082), ("power_over", 200.0)],
)
def test_out_of_range_limit_is_rejected(function: str, limit: float) -> None:
    with pytest.raises(cv.Invalid, match="'limit' is out of range"):
        _validate_alert(_config(function, limit))


def test_conversion_ready_only_needs_no_limit() -> None:
    config = {CONF_MAX_CURRENT: 3.2, CONF_ALERT: {CONF_FUNCTION: "none"}}
    assert _validate_alert(config) is config
