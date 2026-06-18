"""Tests for esphome.espidf.toolchain helpers."""

# pylint: disable=protected-access

import json
import os
from pathlib import Path
from unittest.mock import patch

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


def _setup_build(setup_core: Path) -> tuple[Path, Path]:
    """Point CORE at a build dir; return (compile_commands, idedata cache) paths."""
    CORE.name = "test"
    CORE.build_path = setup_core / "build" / "test"
    compile_commands = CORE.relative_build_path("build", "compile_commands.json")
    cache = CORE.relative_internal_path("idedata", "test.json")
    return compile_commands, cache


def test_get_idedata_returns_none_without_compile_commands(setup_core: Path) -> None:
    """No compile DB yet -> None (rather than an error)."""
    _setup_build(setup_core)
    assert toolchain.get_idedata() is None


def test_get_idedata_generates_and_caches(setup_core: Path) -> None:
    """Generates from the compile DB and writes the cache."""
    compile_commands, cache = _setup_build(setup_core)
    compile_commands.parent.mkdir(parents=True, exist_ok=True)
    compile_commands.write_text("[]")

    with patch(
        "esphome.espidf.idedata.idedata_from_build",
        return_value={"cxx_path": "g++"},
    ) as mock_transform:
        result = toolchain.get_idedata()

    mock_transform.assert_called_once()
    assert result == {"cxx_path": "g++"}
    assert json.loads(cache.read_text()) == {"cxx_path": "g++"}


def test_get_idedata_uses_cache_when_valid(setup_core: Path) -> None:
    """A cache at least as new as the compile DB is reused without regenerating."""
    compile_commands, cache = _setup_build(setup_core)
    compile_commands.parent.mkdir(parents=True, exist_ok=True)
    compile_commands.write_text("[]")
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text('{"cxx_path": "cached"}')
    cc_mtime = compile_commands.stat().st_mtime
    os.utime(cache, (cc_mtime + 1, cc_mtime + 1))

    with patch("esphome.espidf.idedata.idedata_from_build") as mock_transform:
        result = toolchain.get_idedata()

    mock_transform.assert_not_called()
    assert result == {"cxx_path": "cached"}


def test_get_idedata_regenerates_when_compile_commands_newer(setup_core: Path) -> None:
    """A compile DB newer than the cache forces regeneration."""
    compile_commands, cache = _setup_build(setup_core)
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text('{"cxx_path": "stale"}')
    compile_commands.parent.mkdir(parents=True, exist_ok=True)
    compile_commands.write_text("[]")
    cache_mtime = cache.stat().st_mtime
    os.utime(compile_commands, (cache_mtime + 1, cache_mtime + 1))

    with patch(
        "esphome.espidf.idedata.idedata_from_build",
        return_value={"cxx_path": "fresh"},
    ) as mock_transform:
        result = toolchain.get_idedata()

    mock_transform.assert_called_once()
    assert result == {"cxx_path": "fresh"}


def test_get_idedata_regenerates_on_corrupted_cache(setup_core: Path) -> None:
    """An unparseable (but newer) cache falls back to regeneration."""
    compile_commands, cache = _setup_build(setup_core)
    compile_commands.parent.mkdir(parents=True, exist_ok=True)
    compile_commands.write_text("[]")
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text("{not json")
    cc_mtime = compile_commands.stat().st_mtime
    os.utime(cache, (cc_mtime + 1, cc_mtime + 1))

    with patch(
        "esphome.espidf.idedata.idedata_from_build",
        return_value={"cxx_path": "regen"},
    ) as mock_transform:
        result = toolchain.get_idedata()

    mock_transform.assert_called_once()
    assert result == {"cxx_path": "regen"}


def test_get_idf_env_sets_git_ceiling_directories(setup_core: Path) -> None:
    """The IDF env caps git's upward search at the config directory.

    This stops ESP-IDF's `git describe` from walking into an uninitialized or
    corrupt git repo in a parent directory and failing the build.
    """
    toolchain._cache().env.clear()
    # Set IDF_PATH so the framework-install branch is skipped.
    with patch.dict(os.environ, {"IDF_PATH": str(setup_core)}):
        env = toolchain._get_idf_env(version="5.5.4")
    assert CORE.config_dir == setup_core
    assert str(CORE.config_dir) in env["GIT_CEILING_DIRECTORIES"].split(os.pathsep)


def test_get_core_framework_version_from_core_data():
    """The version is read from CORE.data when validation populated it."""
    from esphome.components.esp32.const import KEY_ESP32, KEY_IDF_VERSION
    import esphome.config_validation as cv

    CORE.data = {KEY_ESP32: {KEY_IDF_VERSION: cv.Version(5, 5, 4)}}
    assert toolchain._get_core_framework_version() == "5.5.4"
