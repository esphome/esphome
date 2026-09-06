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
