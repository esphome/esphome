"""Tests for esphome.espidf.toolchain helpers."""

# pylint: disable=protected-access

import json
import os
from pathlib import Path
import subprocess
from unittest.mock import patch

import pytest

from esphome.const import CONF_FRAMEWORK, CONF_SOURCE
from esphome.core import CORE, EsphomeError
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
    prog_path = str(toolchain.get_elf_path())
    assert result == {"cxx_path": "g++", "prog_path": prog_path}
    assert json.loads(cache.read_text()) == {"cxx_path": "g++", "prog_path": prog_path}


def test_get_idedata_uses_cache_when_valid(setup_core: Path) -> None:
    """A cache at least as new as the compile DB is reused without regenerating."""
    compile_commands, cache = _setup_build(setup_core)
    compile_commands.parent.mkdir(parents=True, exist_ok=True)
    compile_commands.write_text("[]")
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text('{"cc_path": "cached-gcc", "cxx_path": "cached"}')
    cc_mtime = compile_commands.stat().st_mtime
    os.utime(cache, (cc_mtime + 1, cc_mtime + 1))

    with patch("esphome.espidf.idedata.idedata_from_build") as mock_transform:
        result = toolchain.get_idedata()

    mock_transform.assert_not_called()
    assert result == {"cc_path": "cached-gcc", "cxx_path": "cached"}


def test_get_idedata_regenerates_cache_without_cc_path(setup_core: Path) -> None:
    """A cache predating cc_path is rebuilt even though it is newer.

    Such a cache stays newer than the compile DB forever, so consumers that
    derive the binutils paths from cc_path would keep failing on it.
    """
    compile_commands, cache = _setup_build(setup_core)
    compile_commands.parent.mkdir(parents=True, exist_ok=True)
    compile_commands.write_text("[]")
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text('{"cxx_path": "cached"}')
    cc_mtime = compile_commands.stat().st_mtime
    os.utime(cache, (cc_mtime + 1, cc_mtime + 1))

    with patch(
        "esphome.espidf.idedata.idedata_from_build",
        return_value={"cc_path": "gcc", "cxx_path": "g++"},
    ) as mock_transform:
        result = toolchain.get_idedata()

    mock_transform.assert_called_once()
    assert result["cc_path"] == "gcc"


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
    assert result == {"cxx_path": "fresh", "prog_path": str(toolchain.get_elf_path())}


@pytest.mark.parametrize("cached", ['"cc_path is a string"', "[]", "42"])
def test_get_idedata_regenerates_on_non_dict_cache(
    setup_core: Path, cached: str
) -> None:
    """A newer cache holding valid JSON that is not an object is regenerated.

    A bare string would otherwise pass the cc_path check by substring and be
    handed to consumers expecting a dict.
    """
    compile_commands, cache = _setup_build(setup_core)
    compile_commands.parent.mkdir(parents=True, exist_ok=True)
    compile_commands.write_text("[]")
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text(cached)
    cc_mtime = compile_commands.stat().st_mtime
    os.utime(cache, (cc_mtime + 1, cc_mtime + 1))

    with patch(
        "esphome.espidf.idedata.idedata_from_build",
        return_value={"cc_path": "gcc", "cxx_path": "g++"},
    ) as mock_transform:
        result = toolchain.get_idedata()

    mock_transform.assert_called_once()
    assert isinstance(result, dict)


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
    assert result == {"cxx_path": "regen", "prog_path": str(toolchain.get_elf_path())}


def test_get_idedata_prog_path_points_at_firmware_elf(setup_core: Path) -> None:
    """The idedata exposes prog_path (the ELF) so consumers like build-action
    can locate firmware.factory.bin / firmware.ota.bin as its siblings."""
    compile_commands, _ = _setup_build(setup_core)
    compile_commands.parent.mkdir(parents=True, exist_ok=True)
    compile_commands.write_text("[]")

    with patch(
        "esphome.espidf.idedata.idedata_from_build",
        return_value={"cxx_path": "g++"},
    ):
        result = toolchain.get_idedata()

    # Use Path semantics so the contract holds on Windows too (backslashes).
    prog_path = Path(result["prog_path"])
    assert prog_path.name == "firmware.elf"
    assert prog_path.parent.name == "build"


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


def test_get_cmake_output_without_build_dir(setup_core: Path) -> None:
    """A build dir that was never created raises EsphomeError.

    Without this, subprocess.run(cwd=build_dir) raises FileNotFoundError, which
    the log stack-trace decoder doesn't recognise as a decode failure.
    """
    _setup_build(setup_core)
    build_dir = CORE.relative_build_path("build")
    assert not build_dir.exists()

    with pytest.raises(EsphomeError, match="No ESP-IDF build found"):
        toolchain._get_cmake_output(build_dir)


def test_get_cmake_output_without_cmake_cache(setup_core: Path) -> None:
    """A build dir that exists but was never configured raises EsphomeError."""
    _setup_build(setup_core)
    build_dir = CORE.relative_build_path("build")
    build_dir.mkdir(parents=True)

    with pytest.raises(EsphomeError, match="No ESP-IDF build found"):
        toolchain._get_cmake_output(build_dir)


def test_get_cmake_output_with_configured_build(setup_core: Path) -> None:
    """A configured build still runs cmake and caches the output.

    The missing-build guard must not get in the way of a real build.
    """
    _setup_build(setup_core)
    build_dir = CORE.relative_build_path("build")
    build_dir.mkdir(parents=True)
    (build_dir / "CMakeCache.txt").write_text("")

    completed = subprocess.CompletedProcess(
        args=[], returncode=0, stdout="CMAKE_ADDR2LINE:FILEPATH=/tool/addr2line\n"
    )
    with (
        patch.object(toolchain, "_get_idf_env", return_value={}),
        patch.object(toolchain.subprocess, "run", return_value=completed) as mock_run,
    ):
        assert toolchain._get_cmake_output(build_dir) == completed.stdout
        # Second call is served from the cache rather than re-running cmake.
        assert toolchain._get_cmake_output(build_dir) == completed.stdout

    mock_run.assert_called_once()
    assert toolchain._get_cmake_tool_path("CMAKE_ADDR2LINE") == Path("/tool/addr2line")


def test_get_cmake_output_missing_build_does_not_resolve_idf_env(
    setup_core: Path,
) -> None:
    """The build check runs before the env is resolved.

    Resolving the env calls check_esp_idf_install(), which can download and
    extract the whole framework. A doomed call must never start that.
    """
    _setup_build(setup_core)
    build_dir = CORE.relative_build_path("build")

    with (
        patch.object(toolchain, "_get_idf_env") as mock_env,
        patch.object(toolchain.subprocess, "run") as mock_run,
        pytest.raises(EsphomeError),
    ):
        toolchain._get_cmake_output(build_dir)

    mock_env.assert_not_called()
    mock_run.assert_not_called()


def test_get_core_framework_version_from_core_data():
    """The version is read from CORE.data when validation populated it."""
    from esphome.components.esp32.const import KEY_ESP32, KEY_IDF_VERSION
    import esphome.config_validation as cv

    CORE.data = {KEY_ESP32: {KEY_IDF_VERSION: cv.Version(5, 5, 4)}}
    assert toolchain._get_core_framework_version() == "5.5.4"
