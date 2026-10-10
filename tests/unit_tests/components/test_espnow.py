"""Tests for the espnow component's final validation."""

import pytest

from esphome.components.esp32.const import (
    VARIANT_ESP32C3,
    VARIANT_ESP32H2,
    VARIANT_ESP32P4,
)
from esphome.components.espnow import CONFIG_SCHEMA, _validate_variant
import esphome.config_validation as cv
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType


def _run(
    monkeypatch,
    variant: str,
    full_config: dict,
    config: ConfigType,
    platform: str = PLATFORM_ESP32,
) -> ConfigType:
    monkeypatch.setattr("esphome.components.espnow.get_esp32_variant", lambda: variant)
    monkeypatch.setitem(CORE.data, KEY_CORE, {KEY_TARGET_PLATFORM: platform})
    token = fv.full_config.set(full_config)
    try:
        return _validate_variant(config)
    finally:
        fv.full_config.reset(token)


def test_variant_with_native_wifi_passes(monkeypatch) -> None:
    """A variant with a native Wi-Fi PHY needs no shim; config passes through."""
    config = {"id": "espnow"}
    assert _run(monkeypatch, VARIANT_ESP32C3, {}, config) is config


def test_radioless_non_p4_variant_rejected(monkeypatch) -> None:
    """Radio-less variants without any ESP-NOW path are rejected outright."""
    with pytest.raises(cv.Invalid, match="not supported"):
        _run(monkeypatch, VARIANT_ESP32H2, {}, {})


def test_p4_without_esp32_hosted_rejected(monkeypatch) -> None:
    """The P4 needs the esp32_hosted shim to supply the esp_now_* symbols."""
    with pytest.raises(cv.Invalid, match="esp32_hosted"):
        _run(monkeypatch, VARIANT_ESP32P4, {}, {})


def test_p4_with_esp32_hosted_passes(monkeypatch) -> None:
    """The P4 with esp32_hosted present validates; config passes through."""
    config = {"id": "espnow"}
    assert _run(monkeypatch, VARIANT_ESP32P4, {"esp32_hosted": {}}, config) is config


def test_esp8266_skips_variant_check(monkeypatch) -> None:
    """The ESP8266 has one Wi-Fi radio and no variants, so the check does not run."""
    config = {"id": "espnow"}
    assert _run(monkeypatch, "", {}, config, platform=PLATFORM_ESP8266) is config


def _esp8266_arduino(monkeypatch) -> None:
    monkeypatch.setitem(
        CORE.data,
        KEY_CORE,
        {KEY_TARGET_PLATFORM: PLATFORM_ESP8266, KEY_TARGET_FRAMEWORK: "arduino"},
    )


def test_esp8266_accepts_the_v1_payload_size(monkeypatch) -> None:
    _esp8266_arduino(monkeypatch)
    assert (
        CONFIG_SCHEMA({"channel": 1, "max_payload_size": 250})["max_payload_size"]
        == 250
    )


def test_esp8266_rejects_v2_payload_sizes(monkeypatch) -> None:
    """The NONOS SDK only speaks ESP-NOW v1, so larger frames are refused at validation."""
    _esp8266_arduino(monkeypatch)
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"channel": 1, "max_payload_size": 251})


def test_esp8266_rejects_on_broadcast(monkeypatch) -> None:
    """The NONOS SDK does not report a destination address, so broadcasts cannot be told apart."""
    _esp8266_arduino(monkeypatch)
    with pytest.raises(cv.Invalid, match="only available on"):
        CONFIG_SCHEMA({"channel": 1, "on_broadcast": []})
