"""request_service_enable_disable() only opts in on platforms whose mDNS stack
can add and remove services after setup, and tells the caller so."""

import pytest

from esphome.components import mdns
from esphome.const import CONF_DISABLED, PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable

DEFINE = "USE_MDNS_SUPPORTS_ENABLE_DISABLE"


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def _set_config(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
    config: dict,
) -> None:
    set_core_config(platform_framework)
    CORE.config = config


@pytest.mark.parametrize(
    "platform_framework",
    [PlatformFramework.ESP32_IDF, PlatformFramework.ESP32_ARDUINO],
)
def test_esp32_adds_define_and_keeps_services_stored(
    set_core_config: SetCoreConfigCallable, platform_framework: PlatformFramework
) -> None:
    _set_config(set_core_config, platform_framework, {"mdns": {CONF_DISABLED: False}})

    assert mdns.request_service_enable_disable() is True
    # Disabled services must stay stored so they can be re-registered later.
    assert {DEFINE, "USE_MDNS_STORE_SERVICES"} <= _defines()


@pytest.mark.parametrize(
    "platform_framework",
    [PlatformFramework.ESP8266_ARDUINO, PlatformFramework.RP2_ARDUINO],
)
def test_other_platforms_return_false(
    set_core_config: SetCoreConfigCallable, platform_framework: PlatformFramework
) -> None:
    _set_config(set_core_config, platform_framework, {"mdns": {CONF_DISABLED: False}})

    assert mdns.request_service_enable_disable() is False
    assert DEFINE not in _defines()


@pytest.mark.parametrize(
    "config",
    [
        pytest.param({}, id="no_mdns"),
        pytest.param({"mdns": {CONF_DISABLED: True}}, id="mdns_disabled"),
        pytest.param(
            {"mdns": {CONF_DISABLED: False}, "openthread": {}}, id="openthread"
        ),
    ],
)
def test_esp32_returns_false_when_services_cannot_be_toggled(
    set_core_config: SetCoreConfigCallable, config: dict
) -> None:
    _set_config(set_core_config, PlatformFramework.ESP32_IDF, config)

    assert mdns.request_service_enable_disable() is False
    assert DEFINE not in _defines()
