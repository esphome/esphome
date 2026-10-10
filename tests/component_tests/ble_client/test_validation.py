"""Tests for ble_client config validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.ble_client import (
    CONF_DESCRIPTOR_UUID,
    CONF_ON_NOTIFY,
    notify_from_on_notify,
    validate_descriptor_not_notify,
)
from esphome.components.ble_client.sensor import CONFIG_SCHEMA as SENSOR_SCHEMA
from esphome.components.ble_client.text_sensor import (
    CONFIG_SCHEMA as TEXT_SENSOR_SCHEMA,
)
from esphome.const import (
    CONF_CHARACTERISTIC_UUID,
    CONF_NAME,
    CONF_NOTIFY,
    CONF_SERVICE_UUID,
    CONF_TYPE,
    PlatformFramework,
)
from esphome.core import CORE
from esphome.types import ConfigType

from ..types import SetCoreConfigCallable


@pytest.fixture(autouse=True)
def esp32_platform(set_core_config: SetCoreConfigCallable) -> None:
    # The raw-gattc node family gates through BLE_CLIENT_SCHEMA's
    # _legacy_engine_only choke point; these schema tests exercise the esp32 arm.
    set_core_config(PlatformFramework.ESP32_IDF)


DESCRIPTOR_CONFIG: ConfigType = {
    CONF_NAME: "test",
    CONF_SERVICE_UUID: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E",
    CONF_CHARACTERISTIC_UUID: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
    CONF_DESCRIPTOR_UUID: "2902",
}


def test_notify_with_descriptor_uuid_rejected() -> None:
    config: ConfigType = {CONF_NOTIFY: True, CONF_DESCRIPTOR_UUID: "2902"}
    with pytest.raises(cv.Invalid, match="cannot send notifications"):
        validate_descriptor_not_notify(config)


def test_on_notify_with_descriptor_uuid_rejected() -> None:
    config: ConfigType = {
        CONF_NOTIFY: False,
        CONF_ON_NOTIFY: [{}],
        CONF_DESCRIPTOR_UUID: "2902",
    }
    with pytest.raises(cv.Invalid, match="cannot send notifications"):
        validate_descriptor_not_notify(config)


def test_descriptor_uuid_without_notify_allowed() -> None:
    config: ConfigType = {CONF_NOTIFY: False, CONF_DESCRIPTOR_UUID: "2902"}
    assert validate_descriptor_not_notify(config) is config


def test_notify_without_descriptor_uuid_allowed() -> None:
    config: ConfigType = {CONF_NOTIFY: True}
    assert validate_descriptor_not_notify(config) is config


def test_sensor_schema_rejects_notify_with_descriptor() -> None:
    config = {**DESCRIPTOR_CONFIG, CONF_TYPE: "characteristic", CONF_NOTIFY: True}
    with pytest.raises(cv.Invalid, match="cannot send notifications"):
        SENSOR_SCHEMA(config)


def test_text_sensor_schema_rejects_notify_with_descriptor() -> None:
    config = {**DESCRIPTOR_CONFIG, CONF_NOTIFY: True}
    with pytest.raises(cv.Invalid, match="cannot send notifications"):
        TEXT_SENSOR_SCHEMA(config)


def test_sensor_schema_allows_descriptor_polling() -> None:
    assert SENSOR_SCHEMA({**DESCRIPTOR_CONFIG, CONF_TYPE: "characteristic"})


def test_text_sensor_schema_allows_descriptor_polling() -> None:
    assert TEXT_SENSOR_SCHEMA(dict(DESCRIPTOR_CONFIG))


def test_on_notify_implies_notify() -> None:
    config: ConfigType = {CONF_NOTIFY: False, CONF_ON_NOTIFY: [{}]}
    assert notify_from_on_notify(config)[CONF_NOTIFY] is True


def test_notify_unchanged_without_on_notify() -> None:
    config: ConfigType = {CONF_NOTIFY: False}
    assert notify_from_on_notify(config)[CONF_NOTIFY] is False


def test_legacy_node_choke_point_rejects_other_platforms(
    set_core_config: SetCoreConfigCallable,
) -> None:
    from esphome.components import ble_client
    from esphome.core import ID

    set_core_config(PlatformFramework.RP2_ARDUINO)
    with pytest.raises(cv.Invalid, match="not been migrated"):
        ble_client._legacy_engine_only(ID("x"))
    # Through the public schema too, so removing the cv.All wiring fails here.
    with pytest.raises(cv.Invalid, match="not been migrated"):
        ble_client.BLE_CLIENT_SCHEMA({})


def test_neutral_arm_rejects_esp32_only_keys(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # Pins the schema split's rejection side: the legacy-only keys must not
    # leak into the neutral arm. The hub is registered so the extra key is
    # the only error - without it the missing-tracker error would satisfy
    # the raises vacuously.
    from esphome.components import ble_client, ble_device_base

    set_core_config(PlatformFramework.RP2_ARDUINO)
    ble_device_base.register_hub_provider("rp2_ble_tracker")
    CORE.loaded_integrations.add("rp2_ble_tracker")
    for key in ("name", "on_passkey_request", "on_passkey_notification"):
        with pytest.raises(cv.Invalid, match="extra keys not allowed"):
            ble_client.CONFIG_SCHEMA({"mac_address": "AA:BB:CC:DD:EE:FF", key: "x"})


def test_security_actions_reject_platforms_without_the_feature(
    set_core_config: SetCoreConfigCallable,
) -> None:
    from esphome.components import ble_client

    set_core_config(PlatformFramework.RP2_ARDUINO)
    for schema in (
        ble_client.BLE_PASSKEY_REPLY_ACTION_SCHEMA,
        ble_client.BLE_NUMERIC_COMPARISON_REPLY_ACTION_SCHEMA,
        ble_client.BLE_REMOVE_BOND_ACTION_SCHEMA,
    ):
        with pytest.raises(cv.Invalid, match="'security' feature, which rp2"):
            schema({})


def test_node_schema_passes_on_every_gatt_platform(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # The neutral node schema carries no engine gate: raw_gattc components
    # stay choked, gatt_node components validate wherever ble_client does.
    from esphome.components import ble_client

    for pf in (PlatformFramework.ESP32_IDF, PlatformFramework.RP2_ARDUINO):
        set_core_config(pf)
        assert ble_client.NODE_BLE_CLIENT_SCHEMA({})


def test_feature_error_names_the_available_features(
    set_core_config: SetCoreConfigCallable,
) -> None:
    from esphome.components import ble_client
    from esphome.core import ID

    set_core_config(PlatformFramework.RP2_ARDUINO)
    with pytest.raises(cv.Invalid, match="provides: gatt_node"):
        ble_client._legacy_engine_only(ID("x"))
