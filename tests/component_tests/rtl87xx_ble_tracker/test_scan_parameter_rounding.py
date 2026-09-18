"""The controller programs whole milliseconds, so the shared 0.625 ms validator
is not enough: a pair it accepts can still collapse to a 100 % duty cycle after
both values round up."""

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_SCAN_PARAMETERS, CONF_WINDOW
from esphome.components.rtl87xx_ble_tracker import CONFIG_SCHEMA, _validate_rounded_pair
from esphome.const import CONF_INTERVAL
from esphome.core import TimePeriodMilliseconds
from esphome.types import ConfigType


def _config(interval_us: int, window_us: int) -> ConfigType:
    return {
        CONF_SCAN_PARAMETERS: {
            CONF_INTERVAL: TimePeriodMilliseconds(microseconds=interval_us),
            CONF_WINDOW: TimePeriodMilliseconds(microseconds=window_us),
        }
    }


def test_rounding_collapse_rejected() -> None:
    # 0.625 ms apart - fine on the shared grid, both round up to 100 ms here.
    with pytest.raises(cv.Invalid, match="100 % duty cycle"):
        _validate_rounded_pair(_config(100_000, 99_375))


def test_validator_is_wired_into_the_schema() -> None:
    # The cases above call the validator directly, so they would still pass if
    # it were dropped from CONFIG_SCHEMA; this pins that it actually runs.
    with pytest.raises(cv.Invalid, match="100 % duty cycle"):
        CONFIG_SCHEMA(
            {
                CONF_SCAN_PARAMETERS: {
                    CONF_INTERVAL: "100ms",
                    CONF_WINDOW: "99375us",
                }
            }
        )


def test_explicitly_equal_pair_allowed() -> None:
    # An explicit window == interval is continuous scanning by request, which
    # the shared validator allows; rounding must not make it stricter.
    _validate_rounded_pair(_config(100_000, 100_000))


def test_separated_pair_allowed() -> None:
    _validate_rounded_pair(_config(100_000, 30_000))
