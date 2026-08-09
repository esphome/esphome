"""Unit tests for esphome.mqtt module."""

from __future__ import annotations

from unittest.mock import Mock, patch

import pytest

from esphome.const import (
    CONF_BROKER,
    CONF_ESPHOME,
    CONF_MQTT,
    CONF_NAME,
    CONF_TOPIC_PREFIX,
)
from esphome.core import EsphomeError
from esphome.mqtt import get_esphome_device_ip, show_logs


@patch("esphome.mqtt.initialize")
def test_show_logs_informs_device(mock_initialize: Mock) -> None:
    """Test that a logs client informs the device."""
    config = {
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
            CONF_TOPIC_PREFIX: "custom-prefix",
        },
        CONF_ESPHOME: {
            CONF_NAME: "test-device",
        },
    }

    show_logs(config)

    assert mock_initialize.call_args.args[1] == ["custom-prefix/debug"]
    on_connect = mock_initialize.call_args.args[3]
    client = Mock()
    on_connect(client, None, None, 0)
    client.publish.assert_called_once_with(
        "esphome/logs/test-device", None, retain=False
    )


def test_get_esphome_device_ip_empty_broker() -> None:
    """Test that get_esphome_device_ip raises EsphomeError when broker is empty."""
    config = {
        CONF_MQTT: {
            CONF_BROKER: "",
        },
        CONF_ESPHOME: {
            CONF_NAME: "test-device",
        },
    }

    with pytest.raises(
        EsphomeError,
        match="Cannot discover IP via MQTT as the broker is not configured",
    ):
        get_esphome_device_ip(config)


def test_get_esphome_device_ip_none_broker() -> None:
    """Test that get_esphome_device_ip raises EsphomeError when broker is None."""
    config = {
        CONF_MQTT: {
            CONF_BROKER: None,
        },
        CONF_ESPHOME: {
            CONF_NAME: "test-device",
        },
    }

    with pytest.raises(
        EsphomeError,
        match="Cannot discover IP via MQTT as the broker is not configured",
    ):
        get_esphome_device_ip(config)


def test_get_esphome_device_ip_missing_mqtt() -> None:
    """Test that get_esphome_device_ip raises EsphomeError when mqtt config is missing."""
    config = {
        CONF_ESPHOME: {
            CONF_NAME: "test-device",
        },
    }

    with pytest.raises(
        EsphomeError,
        match="Cannot discover IP via MQTT as the config does not include the mqtt:",
    ):
        get_esphome_device_ip(config)


def test_get_esphome_device_ip_missing_esphome() -> None:
    """Test that get_esphome_device_ip raises EsphomeError when esphome config is missing."""
    config = {
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
        },
    }

    with pytest.raises(
        EsphomeError,
        match="Cannot discover IP via MQTT as the config does not include the device name:",
    ):
        get_esphome_device_ip(config)


def test_get_esphome_device_ip_missing_name() -> None:
    """Test that get_esphome_device_ip raises EsphomeError when device name is missing."""
    config = {
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
        },
        CONF_ESPHOME: {},
    }

    with pytest.raises(
        EsphomeError,
        match="Cannot discover IP via MQTT as the config does not include the device name:",
    ):
        get_esphome_device_ip(config)
