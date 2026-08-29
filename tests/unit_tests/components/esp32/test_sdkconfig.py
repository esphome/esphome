"""Tests for the esp32 sdkconfig write and its toolchain-gated clean."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.esp32 import _write_sdkconfig
from esphome.components.esp32.const import KEY_SDKCONFIG_OPTIONS
from esphome.const import KEY_CORE, KEY_ESP32, KEY_FRAMEWORK_VERSION, Toolchain
from esphome.core import CORE


@pytest.mark.parametrize(
    ("toolchain", "clean_expected"),
    [(Toolchain.ESP_IDF, False), (Toolchain.PLATFORMIO, True)],
)
def test_write_sdkconfig_cleans_only_on_platformio(
    tmp_path: Path, toolchain: Toolchain, clean_expected: bool
) -> None:
    """A changed sdkconfig forces a full clean only under PlatformIO; the
    esp-idf toolchain reconfigures via has_outdated_files() instead."""
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path
    CORE.toolchain = toolchain
    CORE.data[KEY_ESP32] = {KEY_SDKCONFIG_OPTIONS: {"CONFIG_X": "y"}}
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: "5.5.5"}
    with (
        patch.object(CORE, "name", "test"),
        patch("esphome.components.esp32.clean_build") as clean,
    ):
        _write_sdkconfig()
        assert "CONFIG_X" in CORE.relative_build_path("sdkconfig.test").read_text()
        assert clean.called is clean_expected
        if clean_expected:
            clean.assert_called_once_with(clear_pio_cache=False)
        clean.reset_mock()
        # Unchanged contents: never clean, on either toolchain
        _write_sdkconfig()
        clean.assert_not_called()
