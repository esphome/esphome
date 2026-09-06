"""Tests for the noise-c/libsodium library wiring in the noise component.

On ESP32 (but not the Arduino framework) both libraries build themselves as
native ESP-IDF managed components, so they are declared via add_idf_component()
instead of going through ESPHome's PlatformIO-library converter, on either
toolchain. Elsewhere they still go through that converter via cg.add_library():
on the Arduino framework because arduino-esp32 depends on espressif/libsodium
of its own, and off ESP32 because there are no IDF components at all. This
drives the real to_code() coroutine so every branch of that decision is
exercised end to end, not just mocked.
"""

from __future__ import annotations

import asyncio

import pytest

import esphome.codegen as cg
from esphome.components import esp32, noise
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    Framework,
    Platform,
    Toolchain,
)
from esphome.core import CORE


def _setup_core(platform: Platform, framework: Framework, toolchain: Toolchain) -> None:
    CORE.reset()
    CORE.toolchain = toolchain
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: str(platform),
        KEY_TARGET_FRAMEWORK: str(framework),
    }
    if platform == Platform.ESP32:
        CORE.data[esp32.KEY_ESP32] = {esp32.KEY_VARIANT: "ESP32"}


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
    same on either toolchain, because wireguard splits on the same condition."""
    _setup_core(Platform.ESP32, Framework.ESP_IDF, toolchain)
    idf_calls, lib_calls = _record_calls(monkeypatch)

    asyncio.run(noise.to_code({}))

    assert idf_calls == [
        {"name": "esphome/noise-c", "ref": noise.NOISE_C_VERSION},
        {"name": "esphome/libsodium", "ref": noise.LIBSODIUM_VERSION},
    ]
    assert lib_calls == []


def test_to_code_esp32_arduino_uses_add_library(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """On the Arduino framework arduino-esp32 depends on espressif/libsodium of
    its own, so declaring esphome/libsodium as a managed component too would
    leave the component manager unable to pick between them."""
    _setup_core(Platform.ESP32, Framework.ARDUINO, Toolchain.ESP_IDF)
    idf_calls, lib_calls = _record_calls(monkeypatch)

    asyncio.run(noise.to_code({}))

    assert lib_calls == [
        ("esphome/noise-c", noise.NOISE_C_VERSION),
        ("esphome/libsodium", noise.LIBSODIUM_VERSION),
    ]
    assert idf_calls == []


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
    assert f"esphome/noise-c@{noise.NOISE_C_VERSION}" in (
        repo_root / "platformio.ini"
    ).read_text(encoding="utf-8")
