"""Config-validation tests for the aqi sensor component."""

import pytest
from voluptuous import Invalid

from esphome.components.aqi import CONF_CALCULATION_TYPE, CONF_EXTENDED_RANGE
from esphome.components.aqi.sensor import CONFIG_SCHEMA
from esphome.const import CONF_NAME, CONF_PM_2_5, CONF_PM_10_0, PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


@pytest.fixture(autouse=True)
def _core(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)


def _validate(name: str, **extra: object) -> ConfigType:
    return CONFIG_SCHEMA(
        {CONF_NAME: name, CONF_PM_2_5: "pm25", CONF_PM_10_0: "pm10", **extra}
    )


@pytest.mark.parametrize("extended_range", [True, False])
def test_extended_range_rejected_with_caqi(extended_range: bool) -> None:
    """extended_range has no meaning for CAQI (no spec maximum), whatever its value."""
    with pytest.raises(Invalid, match="CAQI has no maximum"):
        _validate(
            "caqi",
            **{CONF_CALCULATION_TYPE: "CAQI", CONF_EXTENDED_RANGE: extended_range},
        )


def test_extended_range_allowed_with_aqi() -> None:
    """extended_range is valid for the US AQI calculation."""
    config = _validate(
        "aqi", **{CONF_CALCULATION_TYPE: "AQI", CONF_EXTENDED_RANGE: True}
    )
    assert config[CONF_EXTENDED_RANGE] is True


def test_caqi_without_extended_range_ok() -> None:
    """CAQI is fine as long as extended_range is not set."""
    config = _validate("caqi_plain", **{CONF_CALCULATION_TYPE: "CAQI"})
    assert CONF_EXTENDED_RANGE not in config
