"""Unit tests for esphome.mqtt module."""

from __future__ import annotations

import json
import threading
import time
from unittest.mock import MagicMock, patch

import pytest

from esphome.const import CONF_BROKER, CONF_ESPHOME, CONF_MQTT, CONF_NAME
from esphome.core import EsphomeError
from esphome.mqtt import get_esphome_device_ip


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


def _discovery_config() -> dict:
    return {
        CONF_MQTT: {
            CONF_BROKER: "mqtt.local",
        },
        CONF_ESPHOME: {
            CONF_NAME: "test-device",
        },
    }


def test_get_esphome_device_ip_success() -> None:
    """A device answer on the discovery topic returns its IPs."""
    client = MagicMock()

    with patch("esphome.mqtt.prepare", return_value=client) as mock_prepare:
        # Deliver the device answer as soon as the network loop starts
        def deliver(*args, **kwargs):
            msg = MagicMock()
            msg.payload = json.dumps(
                {"name": "test-device", "ip": "10.0.0.5", "ip1": "10.0.0.6"}
            ).encode()
            on_message = mock_prepare.call_args.args[2]
            on_message(client, None, msg)

        client.loop_start.side_effect = deliver

        result = get_esphome_device_ip(_discovery_config())

    assert result == ["10.0.0.5", "10.0.0.6"]
    client.loop_stop.assert_called_once_with()
    client.disconnect.assert_called_once_with()


def test_get_esphome_device_ip_stop_event_aborts_wait() -> None:
    """A set stop event exits the wait loop immediately and raises."""
    stop_event = threading.Event()
    stop_event.set()

    client = MagicMock()
    start = time.monotonic()
    with (
        patch("esphome.mqtt.prepare", return_value=client),
        pytest.raises(EsphomeError, match="Failed to find IP via MQTT"),
    ):
        get_esphome_device_ip(_discovery_config(), stop_event=stop_event)

    # Nowhere near the 25s default timeout
    assert time.monotonic() - start < 5
    client.loop_stop.assert_called_once_with()


def test_get_esphome_device_ip_timeout_raises() -> None:
    """No answer within the timeout raises EsphomeError (default stop event path)."""
    client = MagicMock()
    with (
        patch("esphome.mqtt.prepare", return_value=client),
        pytest.raises(EsphomeError, match="Failed to find IP via MQTT"),
    ):
        get_esphome_device_ip(_discovery_config(), timeout=0.25)

    client.loop_stop.assert_called_once_with()
