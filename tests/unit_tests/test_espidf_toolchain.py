"""Tests for esphome.espidf.toolchain helpers."""

# pylint: disable=protected-access

import os
from pathlib import Path
from unittest.mock import PropertyMock, patch

from esphome.const import CONF_FRAMEWORK, CONF_SOURCE
from esphome.core import CORE
from esphome.espidf import toolchain


def test_get_framework_source_override_no_config():
    """When CORE.config hasn't been set, no override is returned."""
    CORE.config = None
    assert toolchain._get_framework_source_override() is None


def test_get_framework_source_override_no_esp32_section():
    """A config without an esp32 section yields no override."""
    CORE.config = {}
    assert toolchain._get_framework_source_override() is None


def test_get_framework_source_override_no_framework_source():
    """An esp32 section without framework.source yields no override."""
    CORE.config = {"esp32": {CONF_FRAMEWORK: {}}}
    assert toolchain._get_framework_source_override() is None


def test_get_framework_source_override_returns_value():
    """A user-supplied framework source is returned verbatim."""
    url = "https://example.com/esp-idf-v{VERSION}.tar.xz"
    CORE.config = {"esp32": {CONF_FRAMEWORK: {CONF_SOURCE: url}}}
    assert toolchain._get_framework_source_override() == url


def test_get_esphome_esp_idf_paths_forwards_source_override():
    """_get_esphome_esp_idf_paths threads the override into check_esp_idf_install."""
    url = "https://my-mirror/esp-idf-v{VERSION}.tar.xz"
    CORE.config = {"esp32": {CONF_FRAMEWORK: {CONF_SOURCE: url}}}
    # Hit a fresh cache key so check_esp_idf_install is actually called.
    toolchain._cache().paths.clear()
    with patch.object(
        toolchain, "check_esp_idf_install", return_value=("/fw", "/penv")
    ) as mock_install:
        toolchain._get_esphome_esp_idf_paths("5.5.4")
    mock_install.assert_called_once_with("5.5.4", source_url=url)


def test_get_esphome_esp_idf_paths_no_override():
    """When no source override is configured, source_url=None is passed."""
    CORE.config = {}
    toolchain._cache().paths.clear()
    with patch.object(
        toolchain, "check_esp_idf_install", return_value=("/fw", "/penv")
    ) as mock_install:
        toolchain._get_esphome_esp_idf_paths("5.5.4")
    mock_install.assert_called_once_with("5.5.4", source_url=None)


def test_get_sdkconfig_args_uses_sdkconfig_name(tmp_path: Path) -> None:
    """Native-IDF reconfigure should use the canonical sdkconfig name."""
    CORE.build_path = tmp_path
    sdkconfig_path = tmp_path / "sdkconfig.canonical"
    sdkconfig_path.touch()

    with patch.object(
        type(CORE), "sdkconfig_name", new_callable=PropertyMock
    ) as mock_sdkconfig_name:
        mock_sdkconfig_name.return_value = "canonical"

        assert toolchain._get_sdkconfig_args() == [
            "-D",
            f"SDKCONFIG={sdkconfig_path}",
        ]


def test_has_outdated_files_uses_sdkconfig_name(tmp_path: Path) -> None:
    """Native-IDF staleness checks should watch the canonical sdkconfig snapshot."""
    CORE.name = "test-device"
    CORE.build_path = tmp_path

    build_config_dir = tmp_path / "build" / "config"
    build_config_dir.mkdir(parents=True)
    (build_config_dir / "sdkconfig.cmake").write_text("set(CONFIG_TEST y)\n")

    cmakecache_txt = tmp_path / "build" / "CMakeCache.txt"
    build_ninja = tmp_path / "build" / "build.ninja"
    idf_component_yml = tmp_path / "src" / "idf_component.yml"
    sdkconfig_internal = tmp_path / "sdkconfig.canonical.esphomeinternal"
    for path in (cmakecache_txt, build_ninja, idf_component_yml, sdkconfig_internal):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("x\n")

    old_time = 1_700_000_000
    new_time = old_time + 10
    for path in (cmakecache_txt, build_ninja, idf_component_yml):
        os.utime(path, (old_time, old_time))
    os.utime(sdkconfig_internal, (new_time, new_time))

    with patch.object(
        type(CORE), "sdkconfig_name", new_callable=PropertyMock
    ) as mock_sdkconfig_name:
        mock_sdkconfig_name.return_value = "canonical"

        assert toolchain.has_outdated_files() is True
