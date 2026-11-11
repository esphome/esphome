"""Config-validation tests for the scd4x sensor component."""

import pytest
from voluptuous import Invalid

from esphome.components.scd4x.sensor import (
    CONF_AUTOMATIC_SELF_CALIBRATION_TARGET,
    CONFIG_SCHEMA,
)
from esphome.const import CONF_AUTOMATIC_SELF_CALIBRATION, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


@pytest.fixture(autouse=True)
def _core(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)


def test_asc_target_with_asc() -> None:
    """The target is accepted while automatic self calibration runs (its default)."""
    config = CONFIG_SCHEMA({CONF_AUTOMATIC_SELF_CALIBRATION_TARGET: 425})
    assert config[CONF_AUTOMATIC_SELF_CALIBRATION_TARGET] == 425


def test_asc_target_without_asc() -> None:
    """The target does nothing with automatic self calibration off, so it is rejected."""
    with pytest.raises(Invalid, match="requires automatic_self_calibration: true"):
        CONFIG_SCHEMA(
            {
                CONF_AUTOMATIC_SELF_CALIBRATION: False,
                CONF_AUTOMATIC_SELF_CALIBRATION_TARGET: 425,
            }
        )


@pytest.mark.parametrize("target", [399, 1001])
def test_asc_target_range(target: int) -> None:
    """The sensor takes targets from 400 to 1000 ppm."""
    with pytest.raises(Invalid):
        CONFIG_SCHEMA({CONF_AUTOMATIC_SELF_CALIBRATION_TARGET: target})
