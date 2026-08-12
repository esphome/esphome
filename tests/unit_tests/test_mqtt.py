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
    # The one-shot discovery client must not inherit the reconnect-forever
    # handler, which would make loop_stop() join the network thread forever
    assert client.on_disconnect is None
    client.loop_stop.assert_called_once_with()
    # Once from on_message on receiving the answer, once from the finally
    assert client.disconnect.call_count == 2


def test_get_esphome_device_ip_preset_stop_event_skips_lookup() -> None:
    """A stop event set before the call returns [] without touching the broker."""
    stop_event = threading.Event()
    stop_event.set()

    with patch("esphome.mqtt.prepare") as mock_prepare:
        result = get_esphome_device_ip(_discovery_config(), stop_event=stop_event)

    assert result == []
    mock_prepare.assert_not_called()


def test_get_esphome_device_ip_stop_event_aborts_wait() -> None:
    """A stop event set mid-wait exits quietly with no addresses."""
    stop_event = threading.Event()
    client = MagicMock()
    # Simulate teardown starting right after the network loop spins up
    client.loop_start.side_effect = stop_event.set

    start = time.monotonic()
    with patch("esphome.mqtt.prepare", return_value=client):
        result = get_esphome_device_ip(_discovery_config(), stop_event=stop_event)

    # An abort is not a failure and must be nowhere near the 25s timeout
    assert result == []
    assert time.monotonic() - start < 5
    client.disconnect.assert_called_once_with()
    client.loop_stop.assert_called_once_with()


def test_get_esphome_device_ip_timeout_raises() -> None:
    """No answer within the timeout raises EsphomeError (default stop event path)."""
    client = MagicMock()
    with (
        patch("esphome.mqtt.prepare", return_value=client),
        pytest.raises(EsphomeError, match="Failed to find IP via MQTT"),
    ):
        get_esphome_device_ip(_discovery_config(), timeout=0.25)

    client.disconnect.assert_called_once_with()
    client.loop_stop.assert_called_once_with()


def test_get_esphome_device_ip_stop_during_connect_skips_wait() -> None:
    """A stop event set while the broker connect is in flight still cleans up."""
    stop_event = threading.Event()
    client = MagicMock()

    def prepare_and_stop(*args):
        stop_event.set()
        return client

    with patch("esphome.mqtt.prepare", side_effect=prepare_and_stop):
        result = get_esphome_device_ip(_discovery_config(), stop_event=stop_event)

    assert result == []
    client.loop_start.assert_not_called()
    client.disconnect.assert_called_once_with()
    client.loop_stop.assert_called_once_with()
