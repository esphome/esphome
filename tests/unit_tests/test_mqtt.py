"""Unit tests for esphome.mqtt module."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.const import CONF_BROKER, CONF_ESPHOME, CONF_MQTT, CONF_NAME
from esphome.core import EsphomeError
from esphome.mqtt import get_esphome_device_ip


def test_mqtt_light_discovery_does_not_send_deprecated_color_mode_or_brightness() -> (
    None
):
    """MQTT light discovery must not send deprecated 'color_mode' or 'brightness' keys.

    Home Assistant 2025.3+ removed support for these discovery options (see
    home-assistant/core#110682). Sending them causes HA to reject the config.
    We only send supported_color_modes (which includes 'brightness' as a mode).
    """
    # Path to MQTT light C++ implementation (source of truth for discovery payload)
    repo_root = Path(__file__).resolve().parent.parent.parent
    mqtt_light_cpp = repo_root / "esphome" / "components" / "mqtt" / "mqtt_light.cpp"
    assert mqtt_light_cpp.exists(), f"Source file not found: {mqtt_light_cpp}"
    content = mqtt_light_cpp.read_text()

    # Deprecated: root[MQTT_COLOR_MODE] = true in send_discovery
    assert "root[MQTT_COLOR_MODE]" not in content, (
        "MQTT light discovery must not set color_mode (deprecated in HA 2025.3). "
        "See https://github.com/esphome/esphome/issues/13666"
    )

    # Deprecated: root[ESPHOME_F(\"brightness\")] = true (legacy API) in send_discovery.
    # supported_color_modes may still include \"brightness\" as a mode; that is correct.
    assert 'root[ESPHOME_F("brightness")]' not in content, (
        "MQTT light discovery must not set legacy brightness key (deprecated in HA 2025.3). "
        "Use supported_color_modes only. See https://github.com/esphome/esphome/issues/13666"
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
