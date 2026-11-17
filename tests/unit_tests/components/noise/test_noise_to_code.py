"""Tests for the noise-c/libsodium library wiring in the noise component.

Drives the real to_code() so every branch of the managed-versus-converted
decision runs end to end.
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
    """ESP32 + ESP-IDF declares both as managed IDF components on either toolchain."""
    _setup_core(Platform.ESP32, Framework.ESP_IDF, toolchain)
    idf_calls, lib_calls = _record_calls(monkeypatch)

    asyncio.run(noise.to_code({}))

    assert idf_calls == [
        {"name": "esphome/noise-c", "ref": noise.NOISE_C_VERSION},
        {"name": "esphome/libsodium", "ref": noise.LIBSODIUM_VERSION},
    ]
    assert lib_calls == []


def test_to_code_esp32_arduino_platformio_below_idf6_uses_add_library(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The PlatformIO toolchain below IDF 6.0 cannot replace arduino-esp32's
    libsodium, so both stay PlatformIO libraries."""
    _setup_core(
        Platform.ESP32, Framework.ARDUINO, Toolchain.PLATFORMIO, cv.Version(5, 5, 4)
    )
    idf_calls, lib_calls = _record_calls(monkeypatch)

    asyncio.run(noise.to_code({}))

    assert lib_calls == [
        ("esphome/noise-c", noise.NOISE_C_VERSION),
        ("esphome/libsodium", noise.LIBSODIUM_VERSION),
    ]
    assert idf_calls == []


@pytest.mark.parametrize(
    ("toolchain", "idf_version"),
    [
        (Toolchain.ESP_IDF, cv.Version(5, 5, 4)),
        (Toolchain.ESP_IDF, cv.Version(6, 0, 0)),
        (Toolchain.PLATFORMIO, cv.Version(6, 0, 0)),
        (Toolchain.PLATFORMIO, cv.Version(6, 1, 0)),
    ],
)
def test_to_code_esp32_arduino_uses_managed_idf_components(
    toolchain: Toolchain,
    idf_version: cv.Version,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """arduino-esp32's libsodium is replaced through the manifest, so Arduino uses
    managed components too."""
    _setup_core(Platform.ESP32, Framework.ARDUINO, toolchain, idf_version)
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
    """A bump that misses one of the duplicated pins would ship two libsodium versions."""
    from pathlib import Path

    import yaml

    repo_root = Path(__file__).resolve().parents[4]
    manifest = yaml.safe_load(
        (repo_root / "esphome" / "idf_component.yml").read_text(encoding="utf-8")
    )
    deps = manifest["dependencies"]

    assert deps["esphome/noise-c"]["version"] == noise.NOISE_C_VERSION
    assert deps["esphome/libsodium"]["version"] == noise.LIBSODIUM_VERSION
    # arduino-esp32's own libsodium is replaced with the same pin
    assert manifest["overrides"] == [
        {target: {"with": {replacement: {"version": noise.LIBSODIUM_VERSION}}}}
        for target, replacement in esp32.ARDUINO_OVERRIDDEN_IDF_COMPONENTS.items()
    ]
    # Every noise-c pin in platformio.ini, not just one of them
    pins = re.findall(
        r"esphome/noise-c@(\S+)",
        (repo_root / "platformio.ini").read_text(encoding="utf-8"),
    )
    assert set(pins) == {noise.NOISE_C_VERSION}
    assert len(pins) >= 1
