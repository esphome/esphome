"""Tests for esp32's _write_idf_component_yml() managed-component wiring.

A library already declared as a managed IDF component must not also be converted,
or ESP-IDF sees the same requirement twice and refuses to build.
"""

from __future__ import annotations

from pathlib import Path
from unittest.mock import MagicMock

import pytest
import yaml

from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import (
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
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
    """Names registered via add_idf_component are passed as ``managed`` so the
    converter skips them."""
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

    def fake_generate_idf_components(libraries, managed=None, provides=None):
        captured["managed"] = managed
        captured["provides"] = provides
        return [converted]

    monkeypatch.setattr(esp32, "generate_idf_components", fake_generate_idf_components)

    esp32._write_idf_component_yml()

    assert captured["managed"] == {"esphome/noise-c"}
    # ESP-IDF bundles no Arduino libraries, so every name may be fetched
    assert captured["provides"] is None
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
    """No add_idf_component calls means an empty managed set, the old behavior."""
    _setup_core(tmp_path)
    CORE.data[esp32.KEY_ESP32] = {esp32.KEY_COMPONENTS: {}}

    captured: dict[str, set[str] | None] = {}

    def fake_generate_idf_components(libraries, managed=None, provides=None):
        captured["managed"] = managed
        return []

    monkeypatch.setattr(esp32, "generate_idf_components", fake_generate_idf_components)

    esp32._write_idf_component_yml()

    assert captured["managed"] == set()


def test_write_idf_component_yml_marks_arduino_bundled_libraries_provided(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Under Arduino the converter is told which names the core bundles, so a
    manifest dependency on Wire or SPI is never fetched from the registry."""
    _setup_core(tmp_path)
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = str(Framework.ARDUINO)
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version(3, 3, 12)
    CORE.data[esp32.KEY_ESP32] = {
        esp32.KEY_COMPONENTS: {},
        esp32.KEY_IDF_VERSION: cv.Version(5, 5, 4),
        esp32.KEY_ARDUINO_LIBRARIES: {"Wire", "SPI"},
    }

    captured: dict[str, object] = {}

    def fake_generate_idf_components(libraries, managed=None, provides=None):
        captured["provides"] = provides
        return []

    monkeypatch.setattr(esp32, "generate_idf_components", fake_generate_idf_components)

    esp32._write_idf_component_yml()

    provides = captured["provides"]
    assert provides("Wire") and provides("SPI")
    assert not provides("Adafruit BusIO")


def test_arduino_provides_warns_for_a_bundled_library_not_enabled(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A manifest dependency on a bundled Arduino library is never fetched, but
    the library still has to be enabled, so an unenabled one is called out."""
    CORE.reset()
    CORE.data[esp32.KEY_ESP32] = {esp32.KEY_ARDUINO_LIBRARIES: {"Wire"}}
    assert esp32._arduino_provides("Wire")
    assert "Wire" not in caplog.text
    assert esp32._arduino_provides("SPI")
    assert "Arduino library SPI, which is not enabled" in caplog.text
    assert not esp32._arduino_provides("Adafruit BusIO")


@pytest.mark.parametrize(
    ("framework", "toolchain", "version", "bundled"),
    [
        (Framework.ARDUINO, Toolchain.PLATFORMIO, cv.Version(5, 5, 5), True),
        (Framework.ARDUINO, Toolchain.PLATFORMIO, cv.Version(6, 0, 0), False),
        (Framework.ARDUINO, Toolchain.ESP_IDF, cv.Version(5, 5, 5), False),
        (Framework.ESP_IDF, Toolchain.PLATFORMIO, cv.Version(5, 5, 5), False),
    ],
)
def test_arduino_bundles_libsodium(
    framework: Framework,
    toolchain: Toolchain,
    version: cv.Version,
    bundled: bool,
    tmp_path: Path,
) -> None:
    """Only Arduino on the PlatformIO toolchain below IDF 6.0 keeps its own libsodium."""
    _setup_core(tmp_path)
    CORE.toolchain = toolchain
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = str(framework)
    CORE.data[esp32.KEY_ESP32] = {esp32.KEY_IDF_VERSION: version}

    assert esp32.arduino_bundles_libsodium() is bundled


@pytest.mark.parametrize(
    ("version", "libsodium_managed", "overridden"),
    [
        # Nothing in the build uses libsodium, so there is nothing to replace
        (cv.Version(5, 5, 4), False, False),
        (cv.Version(6, 0, 0), False, False),
        # pioarduino's component manager for IDF 5 ignores the "overrides" field
        (cv.Version(5, 5, 4), True, False),
        (cv.Version(6, 0, 0), True, True),
    ],
)
def test_write_idf_component_yml_arduino_overrides_libsodium(
    version: cv.Version,
    libsodium_managed: bool,
    overridden: bool,
    tmp_path: Path,
) -> None:
    """espressif/libsodium is replaced with esphome/libsodium when the build uses it
    and is stubbed otherwise."""
    _setup_core(tmp_path)
    CORE.toolchain = Toolchain.PLATFORMIO
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = str(Framework.ARDUINO)
    CORE.data[esp32.KEY_ESP32] = {
        esp32.KEY_COMPONENTS: {},
        esp32.KEY_IDF_VERSION: version,
        esp32.KEY_ARDUINO_LIBRARIES: set(),
    }
    if libsodium_managed:
        esp32.add_idf_component(name="esphome/libsodium", ref="1.10021.12")

    esp32._write_idf_component_yml()

    manifest = yaml.safe_load(
        (tmp_path / "src" / "idf_component.yml").read_text(encoding="utf-8")
    )
    assert ("espressif/libsodium" in manifest["dependencies"]) is not overridden
    assert "espressif/cbor" in manifest["dependencies"]
    stub_dir = (
        tmp_path
        / "component_stubs"
        / esp32._idf_component_stub_name("espressif__libsodium")
    )
    assert stub_dir.is_dir() is not overridden
    if overridden:
        assert manifest["overrides"] == [
            {
                "espressif/libsodium": {
                    "with": {"esphome/libsodium": {"version": "1.10021.12"}}
                }
            }
        ]
    else:
        assert "overrides" not in manifest


def test_write_idf_component_yml_stub_yields_to_converted_library_of_same_name(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """IDF names a component after its directory and the later registration wins,
    so the espressif/libsodium stub and the converted esphome/libsodium raced on
    path order (#20102). The stub entry points at the converted library instead."""
    _setup_core(tmp_path)
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = str(Framework.ARDUINO)
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version(3, 3, 11)
    CORE.data[esp32.KEY_ESP32] = {
        esp32.KEY_COMPONENTS: {},
        esp32.KEY_IDF_VERSION: cv.Version(5, 5, 5),
        esp32.KEY_ARDUINO_LIBRARIES: set(),
    }
    # A stale stub from an earlier build must not linger beside the real library
    stale_stub = tmp_path / "component_stubs" / "libsodium"
    stale_stub.mkdir(parents=True)

    converted = MagicMock()
    converted.get_sanitized_name.return_value = "esphome/libsodium"
    converted.path = tmp_path / "pio_components" / "esphome" / "libsodium"
    monkeypatch.setattr(
        esp32,
        "generate_idf_components",
        lambda libraries, managed=None, provides=None: [converted],
    )

    esp32._write_idf_component_yml()

    manifest = yaml.safe_load(
        (tmp_path / "src" / "idf_component.yml").read_text(encoding="utf-8")
    )["dependencies"]
    assert manifest["espressif/libsodium"] == {
        "version": "*",
        "override_path": str(converted.path),
    }
    assert manifest["esphome/libsodium"] == {"override_path": str(converted.path)}
    assert not stale_stub.exists()
    # Stubs without a converted namesake are unaffected
    assert (tmp_path / "component_stubs" / "cbor").is_dir()
    assert manifest["espressif/cbor"]["override_path"] == str(
        tmp_path / "component_stubs" / "cbor"
    )
