"""Tests for the noise-c/libsodium library wiring in api's to_code.

On ESP-IDF (not Arduino) both libraries build themselves as native ESP-IDF
managed components, so they are declared via add_idf_component() instead of
going through ESPHome's PlatformIO-library converter. On every other target
(and on the Arduino framework, where arduino-esp32 brings its own bundled
espressif/libsodium) noise-c still goes through the PlatformIO-library
converter via cg.add_library(). This drives the real to_code() coroutine so
both branches of that decision are exercised end to end, not just mocked.
"""

from __future__ import annotations

import asyncio
import base64
from unittest.mock import MagicMock

import pytest

import esphome.codegen as cg
from esphome.components import api, esp32
import esphome.config_validation as cv
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    Framework,
    Platform,
)
from esphome.core import CORE, ID


def _build_config(encryption_key: str) -> dict:
    """A minimal, already-validated api config with encryption enabled."""
    return {
        api.CONF_ID: ID("api_id", is_declaration=True, type=api.APIServer),
        api.CONF_PORT: 6053,
        api.CONF_REBOOT_TIMEOUT: cv.positive_time_period_milliseconds("15min"),
        api.CONF_BATCH_DELAY: cv.positive_time_period_milliseconds("100ms"),
        api.CONF_MAX_CONNECTIONS: 5,
        api.CONF_MAX_SEND_QUEUE: 8,
        api.CONF_CUSTOM_SERVICES: False,
        api.CONF_HOMEASSISTANT_SERVICES: False,
        api.CONF_HOMEASSISTANT_STATES: False,
        api.CONF_ENCRYPTION: {api.CONF_KEY: encryption_key},
    }


@pytest.fixture(name="encryption_key")
def fixture_encryption_key() -> str:
    return base64.b64encode(b"0" * 32).decode()


def _setup_core(platform: Platform, framework: Framework) -> None:
    CORE.reset()
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: str(platform),
        KEY_TARGET_FRAMEWORK: str(framework),
    }
    if platform == Platform.ESP32:
        CORE.data[esp32.KEY_ESP32] = {esp32.KEY_VARIANT: "ESP32"}


def test_to_code_esp32_idf_encryption_uses_managed_idf_components(
    encryption_key: str,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """On ESP32 + ESP-IDF, noise-c and libsodium are declared as managed IDF
    components (add_idf_component), not converted PlatformIO libraries."""
    _setup_core(Platform.ESP32, Framework.ESP_IDF)
    config = _build_config(encryption_key)
    CORE.component_ids.add("api_id")

    add_idf_component_calls: list[dict] = []
    monkeypatch.setattr(
        esp32,
        "add_idf_component",
        lambda **kwargs: add_idf_component_calls.append(kwargs),
    )
    add_library_mock = MagicMock()
    monkeypatch.setattr(cg, "add_library", add_library_mock)

    asyncio.run(api.to_code(config))

    assert add_idf_component_calls == [
        {"name": "esphome/noise-c", "ref": api.NOISE_C_VERSION},
        {"name": "esphome/libsodium", "ref": api.LIBSODIUM_VERSION},
    ]
    add_library_mock.assert_not_called()


def test_to_code_esp32_arduino_encryption_uses_add_library(
    encryption_key: str,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """On the Arduino framework, arduino-esp32 brings its own bundled
    espressif/libsodium, so noise-c must still go through the PlatformIO-
    library converter (cg.add_library) instead of add_idf_component()."""
    _setup_core(Platform.ESP32, Framework.ARDUINO)
    config = _build_config(encryption_key)
    CORE.component_ids.add("api_id")

    add_idf_component_mock = MagicMock()
    monkeypatch.setattr(esp32, "add_idf_component", add_idf_component_mock)
    add_library_calls: list[tuple] = []
    monkeypatch.setattr(
        cg,
        "add_library",
        lambda name, version, repository=None: add_library_calls.append(
            (name, version)
        ),
    )

    asyncio.run(api.to_code(config))

    assert add_library_calls == [("esphome/noise-c", api.NOISE_C_VERSION)]
    add_idf_component_mock.assert_not_called()


def test_to_code_non_esp32_encryption_uses_add_library(
    encryption_key: str,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Off ESP32 entirely (e.g. host), noise-c always goes through the
    PlatformIO-library converter -- add_idf_component is ESP-IDF-only."""
    _setup_core(Platform.HOST, Framework.NATIVE)
    config = _build_config(encryption_key)
    CORE.component_ids.add("api_id")

    add_idf_component_mock = MagicMock()
    monkeypatch.setattr(esp32, "add_idf_component", add_idf_component_mock)
    add_library_calls: list[tuple] = []
    monkeypatch.setattr(
        cg,
        "add_library",
        lambda name, version, repository=None: add_library_calls.append(
            (name, version)
        ),
    )

    asyncio.run(api.to_code(config))

    assert add_library_calls == [("esphome/noise-c", api.NOISE_C_VERSION)]
    add_idf_component_mock.assert_not_called()
