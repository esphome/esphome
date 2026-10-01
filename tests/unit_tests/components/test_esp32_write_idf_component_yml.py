"""Tests for esp32's _write_idf_component_yml() managed-component wiring.

A library that is already declared as a managed IDF component (via
add_idf_component(), e.g. api's noise-c/libsodium) must not also be converted
from a PlatformIO library, or ESP-IDF sees the same requirement declared by
two components and refuses to build. _write_idf_component_yml() passes the
set of already-managed component names to generate_idf_components() so the
converter excludes them.
"""

from __future__ import annotations

from pathlib import Path
from unittest.mock import MagicMock

import pytest

from esphome.components import esp32
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


def _setup_core(tmp_path: Path) -> None:
    CORE.reset()
    CORE.name = "testdevice"
    CORE.build_path = tmp_path
    CORE.toolchain = Toolchain.ESP_IDF
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: str(Platform.ESP32),
        KEY_TARGET_FRAMEWORK: str(Framework.ESP_IDF),
    }


def test_write_idf_component_yml_passes_managed_components(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The names already registered via add_idf_component (e.g. noise-c from
    api's encryption config) are passed through as ``managed`` so the
    PlatformIO-library converter skips them."""
    _setup_core(tmp_path)
    CORE.data[esp32.KEY_ESP32] = {
        esp32.KEY_COMPONENTS: {
            "esphome/noise-c": {
                esp32.KEY_REPO: None,
                esp32.KEY_REF: "0.1.15",
                esp32.KEY_PATH: None,
            },
        },
    }

    captured: dict[str, set[str] | None] = {}

    # A converted (non-managed) library the batch still resolves, so the loop
    # wiring its override_path into the manifest is exercised for real too.
    converted = MagicMock()
    converted.get_sanitized_name.return_value = "esphome/other-lib"
    converted.path = tmp_path / "pio_components" / "other-lib"

    def fake_generate_idf_components(libraries, managed=None):
        captured["managed"] = managed
        return [converted]

    monkeypatch.setattr(esp32, "generate_idf_components", fake_generate_idf_components)

    esp32._write_idf_component_yml()

    assert captured["managed"] == {"esphome/noise-c"}
    # The managed component itself is still written into the manifest deps
    # directly (from KEY_COMPONENTS), just not converted a second time.
    yml_path = tmp_path / "src" / "idf_component.yml"
    assert yml_path.is_file()
    contents = yml_path.read_text(encoding="utf-8")
    assert "esphome/noise-c" in contents
    assert "0.1.15" in contents
    # The converted library the batch DID return is still wired in.
    assert "esphome/other-lib" in contents
    assert str(converted.path) in contents


def test_write_idf_component_yml_empty_managed_when_no_components(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """No managed components registered yet (no add_idf_component calls) ->
    an empty managed set, matching the pre-existing (unfiltered) behavior."""
    _setup_core(tmp_path)
    CORE.data[esp32.KEY_ESP32] = {esp32.KEY_COMPONENTS: {}}

    captured: dict[str, set[str] | None] = {}

    def fake_generate_idf_components(libraries, managed=None):
        captured["managed"] = managed
        return []

    monkeypatch.setattr(esp32, "generate_idf_components", fake_generate_idf_components)

    esp32._write_idf_component_yml()

    assert captured["managed"] == set()


@pytest.mark.parametrize(
    ("version", "libsodium_stubbed"),
    [
        (cv.Version(5, 5, 4), True),
        (cv.Version(5, 99, 99), True),
        (cv.Version(6, 0, 0), False),
        (cv.Version(6, 1, 0), False),
    ],
)
def test_arduino_excluded_idf_components_depends_on_idf_version(
    version: cv.Version, libsodium_stubbed: bool
) -> None:
    """arduino-esp32 only declares espressif/libsodium below IDF 6.0, so it is
    only stubbed there. Unmapped entries are stubbed on every IDF version."""
    CORE.reset()
    CORE.data[esp32.KEY_ESP32] = {esp32.KEY_IDF_VERSION: version}

    excluded = esp32.arduino_excluded_idf_components()

    assert ("espressif__libsodium" in excluded) is libsodium_stubbed
    assert "espressif__cbor" in excluded


@pytest.mark.parametrize(
    ("version", "libsodium_stubbed"),
    [(cv.Version(5, 5, 4), True), (cv.Version(6, 0, 0), False)],
)
def test_write_idf_component_yml_arduino_stubs_follow_idf_version(
    version: cv.Version,
    libsodium_stubbed: bool,
    tmp_path: Path,
) -> None:
    """The Arduino stubs written into the manifest come from the IDF-version
    aware helper: espressif/libsodium is stubbed below IDF 6.0 only, because
    from 6.0 it would clash with ESPHome's own managed esphome/libsodium."""
    _setup_core(tmp_path)
    CORE.toolchain = Toolchain.PLATFORMIO
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = str(Framework.ARDUINO)
    CORE.data[esp32.KEY_ESP32] = {
        esp32.KEY_COMPONENTS: {},
        esp32.KEY_IDF_VERSION: version,
        esp32.KEY_ARDUINO_LIBRARIES: set(),
    }

    esp32._write_idf_component_yml()

    contents = (tmp_path / "src" / "idf_component.yml").read_text(encoding="utf-8")
    assert ("espressif/libsodium" in contents) is libsodium_stubbed
    assert "espressif/cbor" in contents
    stub_dir = (
        tmp_path
        / "component_stubs"
        / esp32._idf_component_stub_name("espressif__libsodium")
    )
    assert stub_dir.is_dir() is libsodium_stubbed
