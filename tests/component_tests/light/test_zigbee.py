"""Regression tests for the Zigbee light hooks."""

from collections.abc import Callable

import pytest

from esphome.components import zigbee
from esphome.components.light import LIGHT_SCHEMA
from esphome.const import CONF_INTERNAL, PlatformFramework
from esphome.core import CORE
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


@pytest.fixture(name="configured_light")
def configured_light_fixture(
    set_core_config: SetCoreConfigCallable,
) -> Callable[..., ConfigType]:
    """Build a real light schema config in an ESP32 + Zigbee context."""
    set_core_config(PlatformFramework.ESP32_IDF)
    CORE.loaded_integrations.update({"esp32", "zigbee"})

    def make_light(**extra: object) -> ConfigType:
        return LIGHT_SCHEMA({"name": "Test light", **extra})

    return make_light


def test_light_schema_accepts_ordinary_light(
    configured_light: Callable[..., ConfigType],
) -> None:
    assert configured_light()["name"] == "Test light"


def test_light_schema_skips_zigbee_validation_for_internal_light(
    configured_light: Callable[..., ConfigType],
) -> None:
    config = configured_light(**{CONF_INTERNAL: True, "report": "enable"})

    assert config["report"] == "enable"


def test_light_schema_accepts_light_without_zigbee(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    assert LIGHT_SCHEMA({"name": "Test light"})["name"] == "Test light"


def test_validate_light_skips_validation_when_zigbee_is_not_loaded() -> None:
    config: ConfigType = {"report": "enable"}

    assert zigbee.validate_light(config) is config
