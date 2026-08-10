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
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_RP2,
)
from esphome.core import CORE
from esphome.types import ConfigType


@pytest.fixture(autouse=True)
def esp32_platform() -> None:
    # The raw-gattc node family gates through BLE_CLIENT_SCHEMA's
    # _legacy_engine_only choke point; these schema tests exercise the esp32 arm.
    CORE.data.setdefault(KEY_CORE, {})[KEY_TARGET_PLATFORM] = PLATFORM_ESP32


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


def test_legacy_node_choke_point_rejects_other_platforms() -> None:
    from esphome.components import ble_client
    from esphome.core import ID

    CORE.data.setdefault(KEY_CORE, {})[KEY_TARGET_PLATFORM] = PLATFORM_RP2
    with pytest.raises(cv.Invalid, match="not been migrated"):
        ble_client._legacy_engine_only(ID("x"))
    # Through the public schema too, so removing the cv.All wiring fails here.
    with pytest.raises(cv.Invalid, match="not been migrated"):
        ble_client.BLE_CLIENT_SCHEMA({})


def test_neutral_arm_rejects_esp32_only_keys() -> None:
    # Pins the schema split's rejection side: the legacy-only keys must not
    # leak into the neutral arm.
    from esphome.components import ble_client

    CORE.data.setdefault(KEY_CORE, {})[KEY_TARGET_PLATFORM] = PLATFORM_RP2
    for key in ("name", "on_passkey_request", "on_passkey_notification"):
        with pytest.raises(cv.Invalid):
            ble_client.CONFIG_SCHEMA({"mac_address": "AA:BB:CC:DD:EE:FF", key: "x"})
