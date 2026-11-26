"""Tests for MQTT ESP8266 TLS fingerprint support."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.components.mqtt import CONF_SSL_FINGERPRINTS
from esphome.const import CONF_PORT, PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable


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
    """Test ESP8266 MQTT keeps a valid TLS fingerprint as written and picks the port."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    CORE.name = "test-mqtt"

    from esphome.components.mqtt import CONFIG_SCHEMA

    config = CONFIG_SCHEMA(
        {"broker": "mqtt.example.test", CONF_SSL_FINGERPRINTS: fingerprints, **extra}
    )

    # Kept as written so a !secret value round-trips through esphome config
    assert config[CONF_SSL_FINGERPRINTS] == (
        fingerprints if isinstance(fingerprints, list) else [fingerprints]
    )
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
