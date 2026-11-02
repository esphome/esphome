"""Tests for the bq25820 current limits, which scale with the sense resistors."""

import pytest

from esphome.components.bq25820 import (
    CONF_AC_SNS_RES,
    CONF_BAT_SNS_RES,
    _current_code,
    _validate_current_limits,
)
from esphome.components.const import CONF_CHARGE_CURRENT, CONF_INPUT_CURRENT_LIMIT
import esphome.config_validation as cv


def test_current_code_scales_with_shunt() -> None:
    # 50 mA per step with the 5 mOhm reference shunt, 125 mA per step at 2 mOhm
    assert _current_code(1.0, 5.0) == 20
    assert _current_code(1.0, 2.0) == 8
    # A 2 mOhm battery shunt needs 2.5 times fewer steps for the same current
    assert _current_code(3.0, 2.0) == 24


def test_charge_current_below_range_is_rejected() -> None:
    config = {CONF_BAT_SNS_RES: 5.0, CONF_AC_SNS_RES: 2.0, CONF_CHARGE_CURRENT: 0.1}
    with pytest.raises(cv.Invalid, match="0.4-20 A with a 5 mΩ bat_sns_res"):
        _validate_current_limits(config)


def test_input_current_range_follows_shunt() -> None:
    # 50 A fits the 2 mOhm reference shunt but not a 4 mOhm one
    config = {CONF_BAT_SNS_RES: 5.0, CONF_AC_SNS_RES: 2.0, CONF_INPUT_CURRENT_LIMIT: 50}
    assert _validate_current_limits(config) is config
    config[CONF_AC_SNS_RES] = 4.0
    with pytest.raises(cv.Invalid, match="0.5-25 A"):
        _validate_current_limits(config)


def test_unset_limits_are_skipped() -> None:
    config = {CONF_BAT_SNS_RES: 5.0, CONF_AC_SNS_RES: 2.0}
    assert _validate_current_limits(config) is config
