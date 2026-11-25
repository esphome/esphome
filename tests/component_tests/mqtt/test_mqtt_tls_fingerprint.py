"""Tests for MQTT ESP8266 TLS fingerprint support."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_SSL_FINGERPRINTS
from esphome.const import CONF_PORT, PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable

ESP8266_ASYNC_MQTT_CLIENT_LIBRARY = "esphome/AsyncMqttClient-esphome"
ESP8266_ASYNC_MQTT_CLIENT_VERSION = "2.2.0"
ESP8266_ESP_ASYNC_TCP_LIBRARY = "esphome/ESP8266AsyncTCP"
ESP8266_ESP_ASYNC_TCP_VERSION = "2.1.2"


@pytest.mark.parametrize(
    ("extra", "fingerprints", "expected_port"),
    [
        # Fingerprint TLS picks the TLS port when none is given
        ({}, [" 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33 "], 8883),
        ({CONF_PORT: 1884}, "00112233445566778899aabbccddeeff00112233", 1884),
    ],
)
def test_mqtt_esp8266_tls_fingerprint_validation(
    set_core_config: SetCoreConfigCallable,
    extra: dict[str, int],
    fingerprints: str | list[str],
    expected_port: int,
) -> None:
    """Test ESP8266 MQTT normalises a TLS fingerprint and picks the port."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    CORE.name = "test-mqtt"

    from esphome.components.mqtt import CONFIG_SCHEMA

    config = CONFIG_SCHEMA(
        {"broker": "mqtt.example.test", CONF_SSL_FINGERPRINTS: fingerprints, **extra}
    )

    assert config[CONF_SSL_FINGERPRINTS] == ["00112233445566778899aabbccddeeff00112233"]
    assert config[CONF_PORT] == expected_port


@pytest.mark.parametrize(
    ("fingerprints", "match"),
    [
        (
            ["invalid-fingerprint"],
            "fingerprint must be a SHA1 hash with 40 hexadecimal digits",
        ),
        (
            [
                "00112233445566778899aabbccddeeff00112233",
                "ffeeddccbbaa99887766554433221100ffeeddcc",
            ],
            "ESP8266 MQTT TLS supports exactly one fingerprint",
        ),
    ],
)
def test_mqtt_esp8266_tls_fingerprint_rejects(
    set_core_config: SetCoreConfigCallable,
    fingerprints: list[str],
    match: str,
) -> None:
    """Test ESP8266 MQTT rejects invalid or multiple TLS fingerprints."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    CORE.name = "test-mqtt"

    from esphome.components.mqtt import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match=match):
        CONFIG_SCHEMA(
            {"broker": "mqtt.example.test", CONF_SSL_FINGERPRINTS: fingerprints}
        )


def test_mqtt_tls_fingerprint_rejects_other_platforms(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test MQTT TLS fingerprints are limited to ESP8266."""
    set_core_config(PlatformFramework.BK72XX_ARDUINO)
    CORE.name = "test-mqtt"

    from esphome.components.mqtt import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match="only available on.*esp8266"):
        CONFIG_SCHEMA(
            {
                "broker": "mqtt.example.test",
                CONF_SSL_FINGERPRINTS: ["00112233445566778899aabbccddeeff00112233"],
            }
        )


def test_mqtt_esp8266_tls_fingerprint_codegen_uses_esp8266_mqtt_libraries(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Test ESP8266 MQTT TLS fingerprint codegen uses the ESP8266 MQTT libraries."""
    main_cpp = generate_main(
        "tests/component_tests/mqtt/test_mqtt_esp8266_tls_fingerprint.yaml"
    )

    assert "mqtt_client->set_broker_port(8883);" in main_cpp
    assert "mqtt_client->set_ssl_fingerprint({" in main_cpp
    assert "mqtt_client->disable_log_message();" in main_cpp
    assert "0x00, 0x11, 0x22, 0x33" in main_cpp
    assert "-DASYNC_TCP_SSL_ENABLED=1" in CORE.build_flags

    async_mqtt_client = CORE.platformio_libraries["AsyncMqttClient-esphome"]
    assert async_mqtt_client.name == ESP8266_ASYNC_MQTT_CLIENT_LIBRARY
    assert async_mqtt_client.version == ESP8266_ASYNC_MQTT_CLIENT_VERSION
    assert async_mqtt_client.repository is None

    async_tcp = CORE.platformio_libraries["ESP8266AsyncTCP"]
    assert async_tcp.name == ESP8266_ESP_ASYNC_TCP_LIBRARY
    assert async_tcp.version == ESP8266_ESP_ASYNC_TCP_VERSION
    assert async_tcp.repository is None
