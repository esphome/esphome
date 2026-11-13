"""Tests for the grundfos_alpha config validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.genibus import CONF_GENIBUS_ID
from esphome.components.grundfos_alpha import CONFIG_SCHEMA, validate_update_interval
from esphome.const import CONF_UPDATE_INTERVAL, PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize("value", ["5s", "30s", "10min"])
def test_update_interval_of_5_s_or_more_is_accepted(value: str) -> None:
    assert validate_update_interval(value) == cv.update_interval(value)


def test_update_interval_never_is_accepted() -> None:
    assert validate_update_interval("never") == cv.update_interval("never")


@pytest.mark.parametrize("value", ["4s", "1s", "500ms"])
def test_update_interval_below_5_s_is_rejected(value: str) -> None:
    with pytest.raises(cv.Invalid):
        validate_update_interval(value)


@pytest.mark.parametrize(
    ("config", "expected"),
    [({}, 30), ({CONF_UPDATE_INTERVAL: "5s"}, 5)],
)
def test_schema_defaults_to_30_s_and_accepts_5_s(
    set_core_config: SetCoreConfigCallable, config: ConfigType, expected: int
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    result = CONFIG_SCHEMA({CONF_GENIBUS_ID: "bus", **config})
    assert result[CONF_UPDATE_INTERVAL] == cv.TimePeriod(seconds=expected)


def test_schema_rejects_an_update_interval_below_5_s(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match="at least 5s"):
        CONFIG_SCHEMA({CONF_GENIBUS_ID: "bus", CONF_UPDATE_INTERVAL: "4s"})
