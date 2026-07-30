"""Tests for ble_client descriptor_uuid and notify validation."""

from __future__ import annotations

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.ble_client.sensor import CONFIG_SCHEMA as SENSOR_SCHEMA
from esphome.components.ble_client.text_sensor import (
    CONFIG_SCHEMA as TEXT_SENSOR_SCHEMA,
)

SERVICE_UUID = "abcd1234-abcd-1234-abcd-abcd12345678"
CHARACTERISTIC_UUID = "abcd1236-abcd-1234-abcd-abcd12345678"
DESCRIPTOR_UUID = "abcd1237-abcd-1234-abcd-abcd12345678"

REJECTED = "cannot be combined with"
# The shared factory takes the entity noun, so each platform must name itself.
REJECTED_SENSOR = "so this sensor would never publish"
REJECTED_TEXT_SENSOR = "so this text sensor would never publish"


def _sensor(name: str, **kwargs: Any) -> dict:
    """Run a characteristic sensor config through the full platform schema."""
    return SENSOR_SCHEMA(
        {
            "type": "characteristic",
            "name": name,
            "ble_client_id": "test_blec",
            "service_uuid": SERVICE_UUID,
            "characteristic_uuid": CHARACTERISTIC_UUID,
            **kwargs,
        }
    )


def _text_sensor(name: str, **kwargs: Any) -> dict:
    """Run a text sensor config through the full platform schema."""
    return TEXT_SENSOR_SCHEMA(
        {
            "name": name,
            "ble_client_id": "test_blec",
            "service_uuid": SERVICE_UUID,
            "characteristic_uuid": CHARACTERISTIC_UUID,
            **kwargs,
        }
    )


# --- accepted configurations ---


def test_sensor_descriptor_without_notify_accepted() -> None:
    """Reading a descriptor is what descriptor_uuid is for.

    The schema normalizes a UUID to upper case, hence folding the validated value.
    """
    config = _sensor("descriptor read", descriptor_uuid=DESCRIPTOR_UUID)
    assert config["descriptor_uuid"].lower() == DESCRIPTOR_UUID
    assert config["notify"] is False


def test_sensor_notify_without_descriptor_accepted() -> None:
    """Subscribing to the characteristic is unaffected by this check."""
    config = _sensor("notifications", notify=True)
    assert config["notify"] is True
    assert "descriptor_uuid" not in config


def test_sensor_descriptor_with_explicit_notify_false_accepted() -> None:
    """An explicit false must not trip the check, only a true one."""
    config = _sensor("explicit false", descriptor_uuid=DESCRIPTOR_UUID, notify=False)
    assert config["descriptor_uuid"].lower() == DESCRIPTOR_UUID


def test_sensor_with_neither_key_given_accepted() -> None:
    """A plain characteristic sensor gives neither key, and defaults notify to false."""
    config = _sensor("plain")
    assert "descriptor_uuid" not in config
    assert config["notify"] is False


def test_rssi_sensor_accepted() -> None:
    """The rssi type carries neither key, so the check must pass it through."""
    config = SENSOR_SCHEMA(
        {"type": "rssi", "name": "rssi", "ble_client_id": "test_blec"}
    )
    assert config["type"] == "rssi"


def test_text_sensor_descriptor_without_notify_accepted() -> None:
    config = _text_sensor("descriptor read", descriptor_uuid=DESCRIPTOR_UUID)
    assert config["descriptor_uuid"].lower() == DESCRIPTOR_UUID
    assert config["notify"] is False


def test_text_sensor_notify_without_descriptor_accepted() -> None:
    config = _text_sensor("notifications", notify=True)
    assert config["notify"] is True


# --- rejected configurations ---


def test_sensor_descriptor_with_notify_rejected() -> None:
    """Regression: this pair validated and produced a sensor that never published.

    descriptor_uuid repoints BLESensor::handle at the descriptor while the
    registration is still issued on the characteristic handle. ESP_GATTC_REG_FOR_NOTIFY_EVT
    then never matches, so node_state never reaches ESTABLISHED, update() refuses to
    poll, and ESP_GATTC_NOTIFY_EVT does not match either.
    """
    with pytest.raises(cv.Invalid, match=REJECTED) as excinfo:
        _sensor("both", descriptor_uuid=DESCRIPTOR_UUID, notify=True)
    assert REJECTED_SENSOR in str(excinfo.value)


def test_text_sensor_descriptor_with_notify_rejected() -> None:
    """The text sensor shares the handle overload, and the same fate."""
    with pytest.raises(cv.Invalid, match=REJECTED) as excinfo:
        _text_sensor("both", descriptor_uuid=DESCRIPTOR_UUID, notify=True)
    assert REJECTED_TEXT_SENSOR in str(excinfo.value)
