"""Validate the Classic controller gate and its codegen request."""

from contextvars import Context
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
from esphome.core import CORE, EsphomeError

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

    with pytest.raises(cv.Invalid, match="original ESP32") as error:
        esp32_ble.final_validation(config)
    assert error.value.path == [esp32_ble.CONF_CLASSIC_BT]


def test_classic_rejects_hosted_controller(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_ARDUINO,
        platform_data={KEY_VARIANT: VARIANT_ESP32, KEY_SDKCONFIG_OPTIONS: {}},
        full_config={esp32_ble.ESP32_HOSTED_DOMAIN: {}},
    )
    config = esp32_ble.CONFIG_SCHEMA({"classic_bt": True})
    with pytest.raises(cv.Invalid, match="local ESP32 controller"):
        esp32_ble.final_validation(config)

    CORE.config = {esp32_ble.DOMAIN: config, esp32_ble.ESP32_HOSTED_DOMAIN: {}}
    # Code generation runs outside the final-validation context.
    with pytest.raises(EsphomeError, match="local ESP32 controller"):
        Context().run(esp32_ble.request_classic_bt)


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


@pytest.mark.parametrize(
    "framework", [PlatformFramework.ESP32_IDF, PlatformFramework.ESP32_ARDUINO]
)
def test_classic_accepts_original_esp32(
    set_core_config: SetCoreConfigCallable,
    framework: PlatformFramework,
) -> None:
    _stage(set_core_config, framework, VARIANT_ESP32)
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
    assert CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]["CONFIG_BT_ACL_CONNECTIONS"] == 5
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
        (PlatformFramework.ESP32_ARDUINO, VARIANT_ESP32C3),
    ],
)
def test_classic_request_rejects_unsupported_targets(
    set_core_config: SetCoreConfigCallable,
    framework: PlatformFramework,
    variant: str,
) -> None:
    _stage(set_core_config, framework, variant)
    CORE.config = {esp32_ble.DOMAIN: esp32_ble.CONFIG_SCHEMA({})}

    with pytest.raises(cv.Invalid, match="original ESP32") as error:
        esp32_ble.validate_classic_bt_target(path=[esp32_ble.CONF_CLASSIC_BT])
    assert error.value.path == [esp32_ble.CONF_CLASSIC_BT]

    with pytest.raises(EsphomeError, match="original ESP32"):
        esp32_ble.request_classic_bt()


@pytest.mark.parametrize(
    ("ble", "classic", "host_capacity"), [(3, 2, 6), (1, 7, 9), (7, 1, 9)]
)
def test_classic_shared_capacity(
    set_core_config: SetCoreConfigCallable, ble: int, classic: int, host_capacity: int
) -> None:
    _stage(set_core_config, PlatformFramework.ESP32_IDF, VARIANT_ESP32)
    config = esp32_ble.CONFIG_SCHEMA(
        {
            "classic_bt": True,
            "max_connections": ble,
            "classic_max_acl_connections": classic,
        }
    )
    esp32_ble.final_validation(config)
    options = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert options["CONFIG_BT_ACL_CONNECTIONS"] == host_capacity
    assert options["CONFIG_BTDM_CTRL_BLE_MAX_CONN"] == ble
    CORE.config = {esp32_ble.DOMAIN: config}
    esp32_ble.request_classic_bt()
    assert options["CONFIG_BT_ACL_CONNECTIONS"] == host_capacity
    assert options["CONFIG_BTDM_CTRL_BR_EDR_MAX_ACL_CONN"] == classic


@pytest.mark.parametrize(("ble", "classic"), [(2, 7), (8, 1), (3, 7)])
def test_classic_rejects_oversubscribed_host(
    set_core_config: SetCoreConfigCallable, ble: int, classic: int
) -> None:
    _stage(set_core_config, PlatformFramework.ESP32_IDF, VARIANT_ESP32)
    config = esp32_ble.CONFIG_SCHEMA(
        {
            "classic_bt": True,
            "max_connections": ble,
            "classic_max_acl_connections": classic,
        }
    )
    with pytest.raises(cv.Invalid, match="maximum is 9"):
        esp32_ble.final_validation(config)


def test_component_request_rechecks_ble_only_budget(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _stage(set_core_config, PlatformFramework.ESP32_IDF, VARIANT_ESP32)
    config = esp32_ble.CONFIG_SCHEMA({"max_connections": 8})
    esp32_ble.final_validation(config)
    options = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert options["CONFIG_BT_ACL_CONNECTIONS"] == 9
    CORE.config = {esp32_ble.DOMAIN: config}
    with pytest.raises(EsphomeError, match="maximum is 9"):
        esp32_ble.request_classic_bt()
    assert options["CONFIG_BT_ACL_CONNECTIONS"] == 9


@pytest.mark.parametrize(
    ("ble_config", "tracker", "effective"),
    [({}, 6, 6), ({"max_connections": 3}, 6, 3), ({}, 0, 3)],
)
def test_classic_budget_with_deprecated_tracker_limit(
    set_core_config: SetCoreConfigCallable,
    ble_config: dict,
    tracker: int,
    effective: int,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: VARIANT_ESP32, KEY_SDKCONFIG_OPTIONS: {}},
        full_config={"esp32_ble_tracker": {"max_connections": tracker}},
    )
    config = esp32_ble.CONFIG_SCHEMA({**ble_config, "classic_bt": True})
    esp32_ble.final_validation(config)
    assert config["max_connections"] == effective
    CORE.config = {esp32_ble.DOMAIN: config}
    esp32_ble.request_classic_bt()
    options = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert options["CONFIG_BTDM_CTRL_BLE_MAX_CONN"] == effective
    assert options["CONFIG_BT_ACL_CONNECTIONS"] == effective + 2
