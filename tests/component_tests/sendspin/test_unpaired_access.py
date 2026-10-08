"""Validation tests for the hub's unpaired access policy."""

import logging

import pytest

from esphome.components.sendspin import (
    CONF_UNPAIRED_ACCESS,
    CONFIG_SCHEMA as HUB_CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA as HUB_FINAL_VALIDATE_SCHEMA,
    request_pairing_code_display_support,
)
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable

HUB_ID = "sendspin_hub_id"
NO_PAIRING_WARNING = "nothing lets a server pair"


def test_hub_key_absent_by_default(set_core_config: SetCoreConfigCallable) -> None:
    """Codegen leaves the hub's built-in default of on in place when the key is not set."""
    set_core_config(PlatformFramework.ESP32_IDF)

    assert CONF_UNPAIRED_ACCESS not in HUB_CONFIG_SCHEMA({"id": HUB_ID})


@pytest.mark.parametrize("value", [True, False])
def test_hub_key_accepted(set_core_config: SetCoreConfigCallable, value: bool) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    config = HUB_CONFIG_SCHEMA({"id": HUB_ID, CONF_UNPAIRED_ACCESS: value})

    assert config[CONF_UNPAIRED_ACCESS] is value


def test_off_without_pairing_method_warns(
    set_core_config: SetCoreConfigCallable, caplog: pytest.LogCaptureFixture
) -> None:
    """With unpaired access off and no way to pair, no server could ever play."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = HUB_CONFIG_SCHEMA({"id": HUB_ID, CONF_UNPAIRED_ACCESS: False})

    with caplog.at_level(logging.WARNING):
        HUB_FINAL_VALIDATE_SCHEMA(config)

    assert NO_PAIRING_WARNING in caplog.text


@pytest.mark.parametrize("method", ["static_pairing_code", "pairing_code"])
def test_off_with_pairing_method_does_not_warn(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
    method: str,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    hub_config = {"id": HUB_ID, CONF_UNPAIRED_ACCESS: False}
    if method == "static_pairing_code":
        hub_config["static_pairing_code"] = "01234567"
    else:
        request_pairing_code_display_support()
    config = HUB_CONFIG_SCHEMA(hub_config)

    with caplog.at_level(logging.WARNING):
        HUB_FINAL_VALIDATE_SCHEMA(config)

    assert NO_PAIRING_WARNING not in caplog.text


def test_on_does_not_warn(
    set_core_config: SetCoreConfigCallable, caplog: pytest.LogCaptureFixture
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config = HUB_CONFIG_SCHEMA({"id": HUB_ID})

    with caplog.at_level(logging.WARNING):
        HUB_FINAL_VALIDATE_SCHEMA(config)

    assert NO_PAIRING_WARNING not in caplog.text
