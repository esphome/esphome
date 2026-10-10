"""Tests for the esp32 sdkconfig write and its toolchain-gated clean."""

from __future__ import annotations

import os
from pathlib import Path
import time
from unittest.mock import patch

import pytest

from esphome.components.esp32 import _write_sdkconfig
from esphome.components.esp32.const import KEY_SDKCONFIG_OPTIONS
from esphome.const import KEY_CORE, KEY_ESP32, KEY_FRAMEWORK_VERSION, Toolchain
from esphome.core import CORE
from esphome.espidf.toolchain import has_outdated_files


def _setup_core(tmp_path: Path, toolchain: Toolchain | None) -> None:
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path
    CORE.toolchain = toolchain
    CORE.data[KEY_ESP32] = {KEY_SDKCONFIG_OPTIONS: {"CONFIG_X": "y"}}
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: "5.5.5"}


def _seed_configured_build(tmp_path: Path) -> None:
    """A settled native build: configure outputs predate what comes next."""
    build = tmp_path / "build"
    (build / "config").mkdir(parents=True)
    (build / "config" / "sdkconfig.h").write_text("")
    (build / "CMakeCache.txt").write_text("")
    (build / "build.ninja").write_text("")
    # Explicitly older than what the test writes next: has_outdated_files()
    # compares st_mtime with a strict >, so same-tick writes would pass
    past = time.time() - 60
    for f in build.rglob("*"):
        os.utime(f, (past, past))


@pytest.mark.parametrize(
    ("toolchain", "clean_expected"),
    [(Toolchain.ESP_IDF, False), (Toolchain.PLATFORMIO, True), (None, True)],
)
def test_write_sdkconfig_cleans_only_on_platformio(
    tmp_path: Path, toolchain: Toolchain | None, clean_expected: bool
) -> None:
    """A changed sdkconfig forces a full clean only under PlatformIO; the
    esp-idf toolchain reconfigures via has_outdated_files() instead; an
    unresolved toolchain fails safe onto the clean."""
    _setup_core(tmp_path, toolchain)
    _seed_configured_build(tmp_path)
    with (
        patch.object(CORE, "name", "test"),
        patch("esphome.components.esp32.clean_build") as clean,
    ):
        _write_sdkconfig()
        assert "CONFIG_X" in CORE.relative_build_path("sdkconfig.test").read_text()
        assert clean.called is clean_expected
        if clean_expected:
            clean.assert_called_once_with(clear_pio_cache=False)
        # The change must still trigger a reconfigure: the internal
        # sdkconfig snapshot is now newer than build/CMakeCache.txt
        assert has_outdated_files() is True
        clean.reset_mock()
        # A settled configure restamps the cache; an unchanged rewrite
        # must then neither clean nor mark the build stale
        future = time.time() + 60
        os.utime(CORE.relative_build_path("build/CMakeCache.txt"), (future, future))
        _write_sdkconfig()
        clean.assert_not_called()
        assert has_outdated_files() is False
