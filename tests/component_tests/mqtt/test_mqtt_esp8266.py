"""Tests for the ESP8266 MQTT backend."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_mqtt_esp8266_codegen_without_fingerprint(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test ESP8266 MQTT without ssl_fingerprints builds no TLS."""
    main_cpp = generate_main(component_config_path("esp8266.yaml"))

    assert "set_ssl_fingerprint" not in main_cpp
    assert "-DASYNC_TCP_SSL_ENABLED=1" not in CORE.build_flags


def test_mqtt_esp8266_tls_fingerprint_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test ESP8266 MQTT TLS fingerprint codegen."""
    main_cpp = generate_main(component_config_path("esp8266_tls_fingerprint.yaml"))

    assert "mqtt_client->set_broker_port(8883);" in main_cpp
    assert "mqtt_client->set_ssl_fingerprint();" in main_cpp
    assert "SSL_FINGERPRINT" in main_cpp
    assert "mqtt_client->disable_log_message();" in main_cpp
    assert "0x00, 0x11, 0x22, 0x33" in main_cpp
    assert "0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF" in main_cpp
    assert "-DASYNC_TCP_SSL_ENABLED=1" in CORE.build_flags
