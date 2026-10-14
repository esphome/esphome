"""request_service_enable_disable() only opts in on platforms whose mDNS stack
can add and remove services after setup, and tells the caller so."""

import pytest

from esphome.components import mdns
from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS
from esphome.components.openthread.const import CONF_BORDER_ROUTER
from esphome.components.psram import KEY_PSRAM_GUARANTEED
from esphome.const import (
    CONF_AP,
    CONF_DISABLED,
    CONF_ETHERNET,
    CONF_NETWORKS,
    CONF_OPENTHREAD,
    CONF_WIFI,
    PlatformFramework,
)
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


def test_esp32_border_router_adds_define(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_config(
        set_core_config,
        PlatformFramework.ESP32_IDF,
        {"mdns": {CONF_DISABLED: False}, "openthread": {"border_router": {}}},
    )

    assert mdns.request_service_enable_disable() is True
    assert {DEFINE, "USE_MDNS_STORE_SERVICES"} <= _defines()


@pytest.mark.parametrize(
    ("config", "service_count", "expected_interfaces"),
    [
        pytest.param({CONF_WIFI: {CONF_NETWORKS: [{}]}}, 3, 1, id="wifi-sta"),
        pytest.param(
            {
                CONF_WIFI: {CONF_NETWORKS: [{}], CONF_AP: {}},
                CONF_ETHERNET: {},
            },
            12,
            3,
            id="wifi-ap-and-ethernet",
        ),
        pytest.param({CONF_WIFI: {CONF_NETWORKS: [], CONF_AP: {}}}, 0, 1, id="wifi-ap"),
        pytest.param({}, 0, 1, id="no-predefined-interface"),
        pytest.param(
            {CONF_OPENTHREAD: {"border_router": {}}},
            1,
            1,
            id="border-router-runtime-services",
        ),
    ],
)
def test_idf_mdns_resource_limits_follow_config(
    set_core_config: SetCoreConfigCallable,
    config: dict,
    service_count: int,
    expected_interfaces: int,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_SDKCONFIG_OPTIONS: {}},
    )
    CORE.config = config

    mdns._add_idf_mdns_resource_options(service_count)

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig["CONFIG_MDNS_MAX_INTERFACES"] == expected_interfaces
    minimum_services = (
        20 if CONF_BORDER_ROUTER in config.get(CONF_OPENTHREAD, {}) else 1
    )
    assert sdkconfig["CONFIG_MDNS_MAX_SERVICES"] == max(minimum_services, service_count)
    assert sdkconfig["CONFIG_MDNS_PREDEF_NETIF_STA"] is (
        CONF_WIFI in config and bool(config[CONF_WIFI].get(CONF_NETWORKS, True))
    )
    assert sdkconfig["CONFIG_MDNS_PREDEF_NETIF_AP"] is (
        CONF_WIFI in config and CONF_AP in config[CONF_WIFI]
    )
    assert sdkconfig["CONFIG_MDNS_PREDEF_NETIF_ETH"] is (CONF_ETHERNET in config)
    assert "CONFIG_MDNS_MEMORY_ALLOC_SPIRAM" not in sdkconfig


def test_idf_mdns_uses_psram_when_guaranteed(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_SDKCONFIG_OPTIONS: {}},
    )
    CORE.config = {}
    CORE.data[KEY_PSRAM_GUARANTEED] = True

    mdns._add_idf_mdns_resource_options(1)

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig["CONFIG_MDNS_MEMORY_ALLOC_SPIRAM"] is True
    assert sdkconfig["CONFIG_MDNS_MEMORY_ALLOC_INTERNAL"] is False


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
