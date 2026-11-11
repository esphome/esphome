"""Validate the Classic controller gate and its codegen request."""

from unittest.mock import patch

import pytest

from esphome import config_validation as cv
from esphome.components import esp32_ble
from esphome.components.esp32 import (
    KEY_SDKCONFIG_OPTIONS,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32S3,
)
from esphome.const import (
    CONF_ESPHOME,
    CONF_NAME_ADD_MAC_SUFFIX,
    KEY_ESP32,
    PlatformFramework,
)
from esphome.core import CORE

from ..types import SetCoreConfigCallable


def _stage(
    set_core_config: SetCoreConfigCallable,
    framework: PlatformFramework,
    variant: str,
) -> None:
    set_core_config(
        framework,
        platform_data={KEY_VARIANT: variant, KEY_SDKCONFIG_OPTIONS: {}},
        full_config={CONF_ESPHOME: {CONF_NAME_ADD_MAC_SUFFIX: False}},
    )


@pytest.mark.parametrize("variant", [VARIANT_ESP32S3, VARIANT_ESP32C3])
def test_classic_rejects_ble_only_variants(
    set_core_config: SetCoreConfigCallable, variant: str
) -> None:
    _stage(set_core_config, PlatformFramework.ESP32_IDF, variant)
    config = esp32_ble.CONFIG_SCHEMA({"classic_bt": True})

    with pytest.raises(cv.Invalid, match="original ESP32 with ESP-IDF") as error:
        esp32_ble.final_validation(config)
    assert error.value.path == [esp32_ble.CONF_CLASSIC_BT]


def test_classic_rejects_arduino(set_core_config: SetCoreConfigCallable) -> None:
    _stage(set_core_config, PlatformFramework.ESP32_ARDUINO, VARIANT_ESP32)
    config = esp32_ble.CONFIG_SCHEMA({"classic_bt": True})

    with pytest.raises(cv.Invalid, match="original ESP32 with ESP-IDF"):
        esp32_ble.final_validation(config)


def test_acl_limit_requires_classic(set_core_config: SetCoreConfigCallable) -> None:
    _stage(set_core_config, PlatformFramework.ESP32_IDF, VARIANT_ESP32)
    config = esp32_ble.CONFIG_SCHEMA({"classic_max_acl_connections": 2})

    with pytest.raises(cv.Invalid, match="requires classic_bt: true") as error:
        esp32_ble.final_validation(config)
    assert error.value.path == [esp32_ble.CONF_CLASSIC_MAX_ACL_CONNECTIONS]


@pytest.mark.parametrize("limit", [0, 8])
def test_acl_limit_rejects_out_of_range(
    set_core_config: SetCoreConfigCallable, limit: int
) -> None:
    _stage(set_core_config, PlatformFramework.ESP32_IDF, VARIANT_ESP32)
    with pytest.raises(cv.Invalid):
        esp32_ble.CONFIG_SCHEMA(
            {"classic_bt": True, "classic_max_acl_connections": limit}
        )


def test_classic_accepts_original_esp32_idf(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _stage(set_core_config, PlatformFramework.ESP32_IDF, VARIANT_ESP32)
    config = esp32_ble.CONFIG_SCHEMA(
        {"classic_bt": True, "classic_max_acl_connections": 2}
    )
    esp32_ble.final_validation(config)
    CORE.config = {esp32_ble.DOMAIN: config}

    with patch.object(esp32_ble.cg, "add_define") as add_define:
        esp32_ble.request_classic_bt()

    add_define.assert_called_once_with("USE_ESP32_BLE_CLASSIC_BT")
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig["CONFIG_BTDM_CTRL_MODE_BTDM"] is True
    assert sdkconfig["CONFIG_BT_CLASSIC_ENABLED"] is True
    assert sdkconfig["CONFIG_BTDM_CTRL_BR_EDR_MAX_ACL_CONN"] == 2
    assert sdkconfig["CONFIG_BTDM_CTRL_BR_EDR_MAX_SYNC_CONN"] == 0


def test_classic_request_works_without_manual_switch(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _stage(set_core_config, PlatformFramework.ESP32_IDF, VARIANT_ESP32)
    CORE.config = {esp32_ble.DOMAIN: esp32_ble.CONFIG_SCHEMA({})}

    with patch.object(esp32_ble.cg, "add_define") as add_define:
        esp32_ble.request_classic_bt()

    add_define.assert_called_once_with("USE_ESP32_BLE_CLASSIC_BT")
    assert (
        CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS][
            "CONFIG_BTDM_CTRL_BR_EDR_MAX_ACL_CONN"
        ]
        == 1
    )


@pytest.mark.parametrize(
    ("framework", "variant"),
    [
        (PlatformFramework.ESP32_IDF, VARIANT_ESP32S3),
        (PlatformFramework.ESP32_ARDUINO, VARIANT_ESP32),
    ],
)
def test_classic_request_rejects_unsupported_targets(
    set_core_config: SetCoreConfigCallable,
    framework: PlatformFramework,
    variant: str,
) -> None:
    _stage(set_core_config, framework, variant)
    CORE.config = {esp32_ble.DOMAIN: esp32_ble.CONFIG_SCHEMA({})}

    with pytest.raises(cv.Invalid, match="original ESP32 with ESP-IDF") as error:
        esp32_ble.validate_classic_bt_target(path=[esp32_ble.CONF_CLASSIC_BT])
    assert error.value.path == [esp32_ble.CONF_CLASSIC_BT]

    with pytest.raises(cv.Invalid, match="original ESP32 with ESP-IDF"):
        esp32_ble.request_classic_bt()
