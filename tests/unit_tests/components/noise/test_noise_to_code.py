"""Tests for the noise-c/libsodium library wiring in the noise component.

On ESP32 both libraries build themselves as native ESP-IDF managed components,
so they are declared via add_idf_component() instead of going through ESPHome's
PlatformIO-library converter, on either toolchain. The one exception is the
Arduino framework below IDF 6.0, where arduino-esp32 depends on
espressif/libsodium of its own; there, and off ESP32 (no IDF components at
all), they still go through that converter via cg.add_library(). This
drives the real to_code() coroutine so every branch of that decision is
exercised end to end, not just mocked.
"""

from __future__ import annotations

import asyncio
import re

import pytest

import esphome.codegen as cg
from esphome.components import esp32, noise
import esphome.config_validation as cv
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    Framework,
    Platform,
    Toolchain,
)
from esphome.core import CORE

DEFAULT_IDF_VERSION = cv.Version(5, 5, 4)


def _setup_core(
    platform: Platform,
    framework: Framework,
    toolchain: Toolchain,
    idf_version: cv.Version = DEFAULT_IDF_VERSION,
) -> None:
    CORE.reset()
    CORE.toolchain = toolchain
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: str(platform),
        KEY_TARGET_FRAMEWORK: str(framework),
    }
    if platform == Platform.ESP32:
        CORE.data[esp32.KEY_ESP32] = {
            esp32.KEY_VARIANT: "ESP32",
            esp32.KEY_IDF_VERSION: idf_version,
        }


def _record_calls(
    monkeypatch: pytest.MonkeyPatch,
) -> tuple[list[dict], list[tuple]]:
    """Capture both wiring paths so each test can assert one ran and one did not."""
    idf_calls: list[dict] = []
    lib_calls: list[tuple] = []
    monkeypatch.setattr(
        esp32, "add_idf_component", lambda **kwargs: idf_calls.append(kwargs)
    )
    monkeypatch.setattr(
        cg,
        "add_library",
        lambda name, version, repository=None: lib_calls.append((name, version)),
    )
    return idf_calls, lib_calls


@pytest.mark.parametrize("toolchain", [Toolchain.ESP_IDF, Toolchain.PLATFORMIO])
def test_to_code_esp32_idf_uses_managed_idf_components(
    toolchain: Toolchain,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """On ESP32 + ESP-IDF both libraries are declared as managed IDF components
    rather than converted PlatformIO libraries. The choice is deliberately the
    same on either toolchain."""
    _setup_core(Platform.ESP32, Framework.ESP_IDF, toolchain)
    idf_calls, lib_calls = _record_calls(monkeypatch)

    asyncio.run(noise.to_code({}))

    assert idf_calls == [
        {"name": "esphome/noise-c", "ref": noise.NOISE_C_VERSION},
        {"name": "esphome/libsodium", "ref": noise.LIBSODIUM_VERSION},
    ]
    assert lib_calls == []


def test_to_code_esp32_arduino_below_idf6_uses_add_library(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """On the Arduino framework below IDF 6.0 arduino-esp32 depends on
    espressif/libsodium of its own, so declaring esphome/libsodium as a managed
    component too would leave the component manager unable to pick between
    them."""
    _setup_core(
        Platform.ESP32, Framework.ARDUINO, Toolchain.ESP_IDF, cv.Version(5, 5, 4)
    )
    idf_calls, lib_calls = _record_calls(monkeypatch)

    asyncio.run(noise.to_code({}))

    assert lib_calls == [
        ("esphome/noise-c", noise.NOISE_C_VERSION),
        ("esphome/libsodium", noise.LIBSODIUM_VERSION),
    ]
    assert idf_calls == []


@pytest.mark.parametrize("idf_version", [cv.Version(6, 0, 0), cv.Version(6, 1, 0)])
def test_to_code_esp32_arduino_idf6_uses_managed_idf_components(
    idf_version: cv.Version,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """From IDF 6.0 arduino-esp32 no longer depends on espressif/libsodium, so
    there is no clash and Arduino uses managed components like ESP-IDF does."""
    _setup_core(Platform.ESP32, Framework.ARDUINO, Toolchain.ESP_IDF, idf_version)
    idf_calls, lib_calls = _record_calls(monkeypatch)

    asyncio.run(noise.to_code({}))

    assert idf_calls == [
        {"name": "esphome/noise-c", "ref": noise.NOISE_C_VERSION},
        {"name": "esphome/libsodium", "ref": noise.LIBSODIUM_VERSION},
    ]
    assert lib_calls == []


def test_to_code_non_esp32_uses_add_library(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Off ESP32 entirely (e.g. host) there are no IDF components at all."""
    _setup_core(Platform.HOST, Framework.NATIVE, Toolchain.PLATFORMIO)
    idf_calls, lib_calls = _record_calls(monkeypatch)

    asyncio.run(noise.to_code({}))

    assert lib_calls == [
        ("esphome/noise-c", noise.NOISE_C_VERSION),
        ("esphome/libsodium", noise.LIBSODIUM_VERSION),
    ]
    assert idf_calls == []


def test_versions_match_the_repo_manifests() -> None:
    """The pins are duplicated in platformio.ini and esphome/idf_component.yml;
    a bump that misses one would ship two different libsodium versions."""
    from pathlib import Path

    import yaml

    repo_root = Path(__file__).resolve().parents[4]
    manifest = yaml.safe_load(
        (repo_root / "esphome" / "idf_component.yml").read_text(encoding="utf-8")
    )
    deps = manifest["dependencies"]

    assert deps["esphome/noise-c"]["version"] == noise.NOISE_C_VERSION
    assert deps["esphome/libsodium"]["version"] == noise.LIBSODIUM_VERSION
    # Both are skipped on Arduino below IDF 6.0, where the PlatformIO library
    # path is used instead (see noise._use_managed_components).
    for name in ("esphome/noise-c", "esphome/libsodium"):
        assert deps[name]["rules"] == [
            {"if": "$ESPHOME_ARDUINO_COMPONENT == 0 || idf_version >= 6.0.0"}
        ]
    # Every noise-c pin in platformio.ini, not just one of them
    pins = re.findall(
        r"esphome/noise-c@(\S+)",
        (repo_root / "platformio.ini").read_text(encoding="utf-8"),
    )
    assert set(pins) == {noise.NOISE_C_VERSION}
    assert len(pins) >= 1
