"""Regression tests for the Zigbee light hooks."""

from collections.abc import Callable
from unittest.mock import AsyncMock, patch

import pytest

from esphome import config_validation as cv
import esphome.codegen as cg
from esphome.components import zigbee
from esphome.components.light import LIGHT_SCHEMA, setup_light_core_
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


@pytest.mark.parametrize(
    ("key", "value"),
    [("report", "enable"), ("endpoint", 1), ("use_device_type", True)],
)
def test_light_schema_rejects_unsupported_zigbee_options(
    configured_light: Callable[..., ConfigType], key: str, value: object
) -> None:
    with pytest.raises(cv.Invalid, match=f"Zigbee option '{key}' is not supported"):
        configured_light(**{key: value})


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


@pytest.mark.asyncio
async def test_setup_passes_light_state_to_zigbee_hook(
    configured_light: Callable[..., ConfigType],
) -> None:
    config = configured_light()
    light_state = cg.MockObj("light_state")
    output = cg.MockObj("light_output")
    hook = AsyncMock()

    with (
        patch("esphome.components.light.lazy_load_setup", hook),
        patch(
            "esphome.components.light.entity_helpers.setup_entity_integrations",
            new_callable=AsyncMock,
        ),
    ):
        await setup_light_core_(light_state, config, output)

    hook.assert_awaited_once_with("zigbee", "setup_light", light_state, config)
