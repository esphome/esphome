"""Tests for MQTT ESP8266 TLS fingerprint support."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_SSL_FINGERPRINTS
from esphome.const import PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable

ESP8266_ASYNC_MQTT_CLIENT_LIBRARY = "esphome/AsyncMqttClient-esphome"
ESP8266_ASYNC_MQTT_CLIENT_VERSION = "2.2.0"
ESP8266_ESP_ASYNC_TCP_LIBRARY = "esphome/ESPAsyncTCP"
ESP8266_ESP_ASYNC_TCP_VERSION = "2.1.0"


def test_mqtt_esp8266_tls_fingerprint_validation(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test ESP8266 MQTT accepts valid TLS fingerprints."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    CORE.name = "test-mqtt"

    from esphome.components.mqtt import CONFIG_SCHEMA

    config = CONFIG_SCHEMA(
        {
            "broker": "mqtt.example.test",
            CONF_SSL_FINGERPRINTS: [
                "00112233445566778899aabbccddeeff00112233",
            ],
        }
    )

    assert config[CONF_SSL_FINGERPRINTS] == [
        "00112233445566778899aabbccddeeff00112233",
    ]


def test_mqtt_esp8266_tls_fingerprint_rejects_invalid_value(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test ESP8266 MQTT rejects invalid TLS fingerprints."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    CORE.name = "test-mqtt"

    from esphome.components.mqtt import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match="fingerprint must be valid SHA1 hash"):
        CONFIG_SCHEMA(
            {
                "broker": "mqtt.example.test",
                CONF_SSL_FINGERPRINTS: ["invalid-fingerprint"],
            }
        )


def test_mqtt_esp8266_tls_rejects_multiple_fingerprints(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test ESP8266 MQTT TLS only accepts one fingerprint."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    CORE.name = "test-mqtt"

    from esphome.components.mqtt import CONFIG_SCHEMA

    with pytest.raises(
        cv.Invalid,
        match="ESP8266 MQTT TLS supports exactly one fingerprint",
    ):
        CONFIG_SCHEMA(
            {
                "broker": "mqtt.example.test",
                CONF_SSL_FINGERPRINTS: [
                    "00112233445566778899aabbccddeeff00112233",
                    "ffeeddccbbaa99887766554433221100ffeeddcc",
                ],
            }
        )


def test_mqtt_esp8266_codegen_without_tls_fingerprint_uses_esp8266_mqtt_libraries(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Test ESP8266 MQTT uses the ESP8266 MQTT libraries without fingerprints."""
    main_cpp = generate_main("tests/component_tests/mqtt/test_mqtt_esp8266.yaml")

    assert "add_ssl_fingerprint" not in main_cpp
    assert "-DASYNC_TCP_SSL_ENABLED=1" not in CORE.build_flags

    async_mqtt_client = CORE.platformio_libraries["AsyncMqttClient-esphome"]
    assert async_mqtt_client.name == ESP8266_ASYNC_MQTT_CLIENT_LIBRARY
    assert async_mqtt_client.version == ESP8266_ASYNC_MQTT_CLIENT_VERSION
    assert async_mqtt_client.repository is None

    async_tcp = CORE.platformio_libraries["ESPAsyncTCP"]
    assert async_tcp.name == ESP8266_ESP_ASYNC_TCP_LIBRARY
    assert async_tcp.version == ESP8266_ESP_ASYNC_TCP_VERSION
    assert async_tcp.repository is None


def test_mqtt_esp8266_tls_fingerprint_codegen_uses_esp8266_mqtt_libraries(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Test ESP8266 MQTT TLS fingerprint codegen uses the ESP8266 MQTT libraries."""
    main_cpp = generate_main(
        "tests/component_tests/mqtt/test_mqtt_esp8266_tls_fingerprint.yaml"
    )

    assert "mqtt_client->set_broker_port(8883);" in main_cpp
    assert "mqtt_client->add_ssl_fingerprint({" in main_cpp
    assert "mqtt_client->disable_log_message();" in main_cpp
    assert "0x00, 0x11, 0x22, 0x33" in main_cpp
    assert "-DASYNC_TCP_SSL_ENABLED=1" in CORE.build_flags

    async_mqtt_client = CORE.platformio_libraries["AsyncMqttClient-esphome"]
    assert async_mqtt_client.name == ESP8266_ASYNC_MQTT_CLIENT_LIBRARY
    assert async_mqtt_client.version == ESP8266_ASYNC_MQTT_CLIENT_VERSION
    assert async_mqtt_client.repository is None

    async_tcp = CORE.platformio_libraries["ESPAsyncTCP"]
    assert async_tcp.name == ESP8266_ESP_ASYNC_TCP_LIBRARY
    assert async_tcp.version == ESP8266_ESP_ASYNC_TCP_VERSION
    assert async_tcp.repository is None
