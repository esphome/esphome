"""Tests for the espnow component's final validation."""

import pytest

from esphome.components.esp32.const import (
    VARIANT_ESP32C3,
    VARIANT_ESP32H2,
    VARIANT_ESP32P4,
)
from esphome.components.espnow import _validate_variant
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.types import ConfigType


def _run(
    monkeypatch, variant: str, full_config: dict, config: ConfigType
) -> ConfigType:
    monkeypatch.setattr("esphome.components.espnow.get_esp32_variant", lambda: variant)
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
