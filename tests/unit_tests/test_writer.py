"""Test writer module functionality."""

from collections.abc import Callable
from contextlib import contextmanager
from dataclasses import dataclass
from datetime import datetime
import json
import os
from pathlib import Path
import stat
from typing import Any
from unittest.mock import MagicMock, patch

import pytest

from esphome.const import (
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RP2040,
    PLATFORM_RTL87XX,
)
from esphome.core import EsphomeError
from esphome.storage_json import StorageJSON
from esphome.writer import (
    CPP_AUTO_GENERATE_BEGIN,
    CPP_AUTO_GENERATE_END,
    CPP_INCLUDE_BEGIN,
    CPP_INCLUDE_END,
    GITIGNORE_CONTENT,
    clean_all,
    clean_build,
    clean_cmake_cache,
    copy_src_tree,
    generate_build_info_data_h,
    get_build_info,
    storage_should_clean,
    storage_should_update_cmake_cache,
    update_storage_json,
    write_cpp,
    write_gitignore,
)


@pytest.fixture
def mock_copy_src_tree():
    """Mock copy_src_tree to avoid side effects during tests."""
    with patch("esphome.writer.copy_src_tree"):
        yield


@pytest.fixture
def create_storage() -> Callable[..., StorageJSON]:
    """Factory fixture to create StorageJSON instances."""

    def _create(
        loaded_integrations: list[str] | None = None, **kwargs: Any
    ) -> StorageJSON:
        return StorageJSON(
            storage_version=kwargs.get("storage_version", 1),
            name=kwargs.get("name", "test"),
            friendly_name=kwargs.get("friendly_name", "Test Device"),
            comment=kwargs.get("comment"),
            esphome_version=kwargs.get("esphome_version", "2025.1.0"),
            src_version=kwargs.get("src_version", 1),
            address=kwargs.get("address", "test.local"),
            web_port=kwargs.get("web_port", 80),
            target_platform=kwargs.get("target_platform", "ESP32"),
            build_path=kwargs.get("build_path", "/build"),
            firmware_bin_path=kwargs.get("firmware_bin_path", "/firmware.bin"),
            loaded_integrations=set(loaded_integrations or []),
            loaded_platforms=kwargs.get("loaded_platforms", set()),
            no_mdns=kwargs.get("no_mdns", False),
            framework=kwargs.get("framework", "arduino"),
            core_platform=kwargs.get("core_platform", "esp32"),
        )

    return _create


def test_storage_should_clean_when_old_is_none(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test that clean is triggered when old storage is None."""
    new = create_storage(loaded_integrations=["api", "wifi"])
    assert storage_should_clean(None, new) is True


def test_storage_should_clean_when_src_version_changes(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test that clean is triggered when src_version changes."""
    old = create_storage(loaded_integrations=["api", "wifi"], src_version=1)
    new = create_storage(loaded_integrations=["api", "wifi"], src_version=2)
    assert storage_should_clean(old, new) is True


def test_storage_should_clean_when_build_path_changes(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test that clean is triggered when build_path changes."""
    old = create_storage(loaded_integrations=["api", "wifi"], build_path="/build1")
    new = create_storage(loaded_integrations=["api", "wifi"], build_path="/build2")
    assert storage_should_clean(old, new) is True


def test_storage_should_clean_when_component_removed(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test that clean is triggered when a component is removed."""
    old = create_storage(
        loaded_integrations=["api", "wifi", "bluetooth_proxy", "esp32_ble_tracker"]
    )
    new = create_storage(loaded_integrations=["api", "wifi", "esp32_ble_tracker"])
    assert storage_should_clean(old, new) is True


def test_storage_should_clean_when_multiple_components_removed(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test that clean is triggered when multiple components are removed."""
    old = create_storage(
        loaded_integrations=["api", "wifi", "ota", "web_server", "logger"]
    )
    new = create_storage(loaded_integrations=["api", "wifi", "logger"])
    assert storage_should_clean(old, new) is True


def test_storage_should_not_clean_when_nothing_changes(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test that clean is not triggered when nothing changes."""
    old = create_storage(loaded_integrations=["api", "wifi", "logger"])
    new = create_storage(loaded_integrations=["api", "wifi", "logger"])
    assert storage_should_clean(old, new) is False


def test_storage_should_not_clean_when_component_added(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test that clean is not triggered when a component is only added."""
    old = create_storage(loaded_integrations=["api", "wifi"])
    new = create_storage(loaded_integrations=["api", "wifi", "ota"])
    assert storage_should_clean(old, new) is False


def test_storage_should_not_clean_when_other_fields_change(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test that clean is not triggered when non-relevant fields change."""
    old = create_storage(
        loaded_integrations=["api", "wifi"],
        friendly_name="Old Name",
        esphome_version="2024.12.0",
    )
    new = create_storage(
        loaded_integrations=["api", "wifi"],
        friendly_name="New Name",
        esphome_version="2025.1.0",
    )
    assert storage_should_clean(old, new) is False


def test_storage_edge_case_empty_integrations(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test edge case when old has integrations but new has none."""
    old = create_storage(loaded_integrations=["api", "wifi"])
    new = create_storage(loaded_integrations=[])
    assert storage_should_clean(old, new) is True


def test_storage_edge_case_from_empty_integrations(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test edge case when old has no integrations but new has some."""
    old = create_storage(loaded_integrations=[])
    new = create_storage(loaded_integrations=["api", "wifi"])
    assert storage_should_clean(old, new) is False


# Tests for storage_should_update_cmake_cache


@pytest.mark.parametrize("framework", ["arduino", "esp-idf"])
def test_storage_should_update_cmake_cache_when_integration_added_esp32(
    create_storage: Callable[..., StorageJSON],
    framework: str,
) -> None:
    """Test cmake cache update triggered when integration added on ESP32."""
    old = create_storage(
        loaded_integrations=["api", "wifi"],
        core_platform=PLATFORM_ESP32,
        framework=framework,
    )
    new = create_storage(
        loaded_integrations=["api", "wifi", "restart"],
        core_platform=PLATFORM_ESP32,
        framework=framework,
    )
    assert storage_should_update_cmake_cache(old, new) is True


def test_storage_should_update_cmake_cache_when_platform_changed_esp32(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test cmake cache update triggered when platforms change on ESP32."""
    old = create_storage(
        loaded_integrations=["api", "wifi"],
        loaded_platforms={"sensor"},
        core_platform=PLATFORM_ESP32,
        framework="arduino",
    )
    new = create_storage(
        loaded_integrations=["api", "wifi"],
        loaded_platforms={"sensor", "binary_sensor"},
        core_platform=PLATFORM_ESP32,
        framework="arduino",
    )
    assert storage_should_update_cmake_cache(old, new) is True


def test_storage_should_not_update_cmake_cache_when_nothing_changes(
    create_storage: Callable[..., StorageJSON],
) -> None:
    """Test cmake cache not updated when nothing changes."""
    old = create_storage(
        loaded_integrations=["api", "wifi"],
        core_platform=PLATFORM_ESP32,
        framework="arduino",
    )
    new = create_storage(
        loaded_integrations=["api", "wifi"],
        core_platform=PLATFORM_ESP32,
        framework="arduino",
    )
    assert storage_should_update_cmake_cache(old, new) is False


@pytest.mark.parametrize(
    "core_platform",
    [PLATFORM_ESP8266, PLATFORM_RP2040, PLATFORM_BK72XX, PLATFORM_RTL87XX],
)
def test_storage_should_not_update_cmake_cache_for_non_esp32(
    create_storage: Callable[..., StorageJSON],
    core_platform: str,
) -> None:
    """Test cmake cache not updated for non-ESP32 platforms."""
    old = create_storage(
        loaded_integrations=["api", "wifi"],
        core_platform=core_platform,
        framework="arduino",
    )
    new = create_storage(
        loaded_integrations=["api", "wifi", "restart"],
        core_platform=core_platform,
        framework="arduino",
    )
    assert storage_should_update_cmake_cache(old, new) is False


@patch("esphome.writer.clean_build")
@patch("esphome.writer.StorageJSON")
@patch("esphome.writer.storage_path")
@patch("esphome.writer.CORE")
def test_update_storage_json_logging_when_old_is_none(
    mock_core: MagicMock,
    mock_storage_path: MagicMock,
    mock_storage_json_class: MagicMock,
    mock_clean_build: MagicMock,
    create_storage: Callable[..., StorageJSON],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test that update_storage_json doesn't crash when old storage is None.

    This is a regression test for the AttributeError that occurred when
    old was None and we tried to access old.loaded_integrations.
    """
    # Setup mocks
    mock_storage_path.return_value = "/test/path"
    mock_storage_json_class.load.return_value = None  # Old storage is None

    new_storage = create_storage(loaded_integrations=["api", "wifi"])
    new_storage.save = MagicMock()  # Mock the save method
    mock_storage_json_class.from_esphome_core.return_value = new_storage

    # Call the function - should not raise AttributeError
    with caplog.at_level("INFO"):
        update_storage_json()

    # Verify clean_build was called
    mock_clean_build.assert_called_once()

    # Verify the correct log message was used (not the component removal message)
    assert "Core config or version changed, cleaning build files..." in caplog.text
    assert "Components removed" not in caplog.text

    # Verify save was called
    new_storage.save.assert_called_once_with("/test/path")


@patch("esphome.writer.clean_build")
@patch("esphome.writer.StorageJSON")
@patch("esphome.writer.storage_path")
@patch("esphome.writer.CORE")
def test_update_storage_json_logging_components_removed(
    mock_core: MagicMock,
    mock_storage_path: MagicMock,
    mock_storage_json_class: MagicMock,
    mock_clean_build: MagicMock,
    create_storage: Callable[..., StorageJSON],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test that update_storage_json logs removed components correctly."""
    # Setup mocks
    mock_storage_path.return_value = "/test/path"

    old_storage = create_storage(loaded_integrations=["api", "wifi", "bluetooth_proxy"])
    new_storage = create_storage(loaded_integrations=["api", "wifi"])
    new_storage.save = MagicMock()  # Mock the save method

    mock_storage_json_class.load.return_value = old_storage
    mock_storage_json_class.from_esphome_core.return_value = new_storage

    # Call the function
    with caplog.at_level("INFO"):
        update_storage_json()

    # Verify clean_build was called
    mock_clean_build.assert_called_once()

    # Verify the correct log message was used with component names
    assert (
        "Components removed (bluetooth_proxy), cleaning build files..." in caplog.text
    )
    assert "Core config or version changed" not in caplog.text

    # Verify save was called
    new_storage.save.assert_called_once_with("/test/path")


@patch("esphome.writer.CORE")
def test_clean_cmake_cache(
    mock_core: MagicMock,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test clean_cmake_cache removes CMakeCache.txt file."""
    # Create directory structure
    pioenvs_dir = tmp_path / ".pioenvs"
    pioenvs_dir.mkdir()
    device_dir = pioenvs_dir / "test_device"
    device_dir.mkdir()
    cmake_cache_file = device_dir / "CMakeCache.txt"
    cmake_cache_file.write_text("# CMake cache file")

    # Setup mocks
    mock_core.relative_pioenvs_path.return_value = pioenvs_dir
    mock_core.name = "test_device"

    # Verify file exists before
    assert cmake_cache_file.exists()

    # Call the function
    with caplog.at_level("INFO"):
        clean_cmake_cache()

    # Verify file was removed
    assert not cmake_cache_file.exists()

    # Verify logging
    assert "Deleting" in caplog.text
    assert "CMakeCache.txt" in caplog.text


@patch("esphome.writer.CORE")
def test_clean_cmake_cache_no_pioenvs_dir(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test clean_cmake_cache when pioenvs directory doesn't exist."""
    # Setup non-existent directory path
    pioenvs_dir = tmp_path / ".pioenvs"

    # Setup mocks
    mock_core.relative_pioenvs_path.return_value = pioenvs_dir

    # Verify directory doesn't exist
    assert not pioenvs_dir.exists()

    # Call the function - should not crash
    clean_cmake_cache()

    # Verify directory still doesn't exist
    assert not pioenvs_dir.exists()


@patch("esphome.writer.CORE")
def test_clean_cmake_cache_no_cmake_file(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test clean_cmake_cache when CMakeCache.txt doesn't exist."""
    # Create directory structure without CMakeCache.txt
    pioenvs_dir = tmp_path / ".pioenvs"
    pioenvs_dir.mkdir()
    device_dir = pioenvs_dir / "test_device"
    device_dir.mkdir()
    cmake_cache_file = device_dir / "CMakeCache.txt"

    # Setup mocks
    mock_core.relative_pioenvs_path.return_value = pioenvs_dir
    mock_core.name = "test_device"

    # Verify file doesn't exist
    assert not cmake_cache_file.exists()

    # Call the function - should not crash
    clean_cmake_cache()

    # Verify file still doesn't exist
    assert not cmake_cache_file.exists()


@patch("esphome.writer.CORE")
def test_clean_build(
    mock_core: MagicMock,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test clean_build removes all build artifacts."""
    # Create directory structure and files
    pioenvs_dir = tmp_path / ".pioenvs"
    pioenvs_dir.mkdir()
    (pioenvs_dir / "test_file.o").write_text("object file")

    piolibdeps_dir = tmp_path / ".piolibdeps"
    piolibdeps_dir.mkdir()
    (piolibdeps_dir / "library").mkdir()

    dependencies_lock = tmp_path / "dependencies.lock"
    dependencies_lock.write_text("lock file")

    # Create PlatformIO cache directory
    platformio_cache_dir = tmp_path / ".platformio" / ".cache"
    platformio_cache_dir.mkdir(parents=True)
    (platformio_cache_dir / "downloads").mkdir()
    (platformio_cache_dir / "http").mkdir()
    (platformio_cache_dir / "tmp").mkdir()
    (platformio_cache_dir / "downloads" / "package.tar.gz").write_text("package")

    # Setup mocks
    mock_core.relative_pioenvs_path.return_value = pioenvs_dir
    mock_core.relative_piolibdeps_path.return_value = piolibdeps_dir
    mock_core.relative_build_path.return_value = dependencies_lock

    # Verify all exist before
    assert pioenvs_dir.exists()
    assert piolibdeps_dir.exists()
    assert dependencies_lock.exists()
    assert platformio_cache_dir.exists()

    # Mock PlatformIO's ProjectConfig cache_dir
    with patch(
        "platformio.project.config.ProjectConfig.get_instance"
    ) as mock_get_instance:
        mock_config = MagicMock()
        mock_get_instance.return_value = mock_config
        mock_config.get.side_effect = (
            lambda section, option: str(platformio_cache_dir)
            if (section, option) == ("platformio", "cache_dir")
            else ""
        )

        # Call the function
        with caplog.at_level("INFO"):
            clean_build()

    # Verify all were removed
    assert not pioenvs_dir.exists()
    assert not piolibdeps_dir.exists()
    assert not dependencies_lock.exists()
    assert not platformio_cache_dir.exists()

    # Verify logging
    assert "Deleting" in caplog.text
    assert ".pioenvs" in caplog.text
    assert ".piolibdeps" in caplog.text
    assert "dependencies.lock" in caplog.text
    assert "PlatformIO cache" in caplog.text


@patch("esphome.writer.CORE")
def test_clean_build_partial_exists(
    mock_core: MagicMock,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test clean_build when only some paths exist."""
    # Create only pioenvs directory
    pioenvs_dir = tmp_path / ".pioenvs"
    pioenvs_dir.mkdir()
    (pioenvs_dir / "test_file.o").write_text("object file")

    piolibdeps_dir = tmp_path / ".piolibdeps"
    dependencies_lock = tmp_path / "dependencies.lock"

    # Setup mocks
    mock_core.relative_pioenvs_path.return_value = pioenvs_dir
    mock_core.relative_piolibdeps_path.return_value = piolibdeps_dir
    mock_core.relative_build_path.return_value = dependencies_lock

    # Verify only pioenvs exists
    assert pioenvs_dir.exists()
    assert not piolibdeps_dir.exists()
    assert not dependencies_lock.exists()

    # Call the function
    with caplog.at_level("INFO"):
        clean_build()

    # Verify only existing path was removed
    assert not pioenvs_dir.exists()
    assert not piolibdeps_dir.exists()
    assert not dependencies_lock.exists()

    # Verify logging - only pioenvs should be logged
    assert "Deleting" in caplog.text
    assert ".pioenvs" in caplog.text
    assert ".piolibdeps" not in caplog.text
    assert "dependencies.lock" not in caplog.text


@patch("esphome.writer.CORE")
def test_clean_build_nothing_exists(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test clean_build when no build artifacts exist."""
    # Setup paths that don't exist
    pioenvs_dir = tmp_path / ".pioenvs"
    piolibdeps_dir = tmp_path / ".piolibdeps"
    dependencies_lock = tmp_path / "dependencies.lock"

    # Setup mocks
    mock_core.relative_pioenvs_path.return_value = pioenvs_dir
    mock_core.relative_piolibdeps_path.return_value = piolibdeps_dir
    mock_core.relative_build_path.return_value = dependencies_lock

    # Verify nothing exists
    assert not pioenvs_dir.exists()
    assert not piolibdeps_dir.exists()
    assert not dependencies_lock.exists()

    # Call the function - should not crash
    clean_build()

    # Verify nothing was created
    assert not pioenvs_dir.exists()
    assert not piolibdeps_dir.exists()
    assert not dependencies_lock.exists()


@patch("esphome.writer.CORE")
def test_clean_build_platformio_not_available(
    mock_core: MagicMock,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test clean_build when PlatformIO is not available."""
    # Create directory structure and files
    pioenvs_dir = tmp_path / ".pioenvs"
    pioenvs_dir.mkdir()

    piolibdeps_dir = tmp_path / ".piolibdeps"
    piolibdeps_dir.mkdir()

    dependencies_lock = tmp_path / "dependencies.lock"
    dependencies_lock.write_text("lock file")

    # Setup mocks
    mock_core.relative_pioenvs_path.return_value = pioenvs_dir
    mock_core.relative_piolibdeps_path.return_value = piolibdeps_dir
    mock_core.relative_build_path.return_value = dependencies_lock

    # Verify all exist before
    assert pioenvs_dir.exists()
    assert piolibdeps_dir.exists()
    assert dependencies_lock.exists()

    # Mock import error for platformio
    with (
        patch.dict("sys.modules", {"platformio.project.config": None}),
        caplog.at_level("INFO"),
    ):
        # Call the function
        clean_build()

    # Verify standard paths were removed but no cache cleaning attempted
    assert not pioenvs_dir.exists()
    assert not piolibdeps_dir.exists()
    assert not dependencies_lock.exists()

    # Verify no cache logging
    assert "PlatformIO cache" not in caplog.text


@patch("esphome.writer.CORE")
def test_clean_build_empty_cache_dir(
    mock_core: MagicMock,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test clean_build when get_project_cache_dir returns empty/whitespace."""
    # Create directory structure and files
    pioenvs_dir = tmp_path / ".pioenvs"
    pioenvs_dir.mkdir()

    # Setup mocks
    mock_core.relative_pioenvs_path.return_value = pioenvs_dir
    mock_core.relative_piolibdeps_path.return_value = tmp_path / ".piolibdeps"
    mock_core.relative_build_path.return_value = tmp_path / "dependencies.lock"

    # Verify pioenvs exists before
    assert pioenvs_dir.exists()

    # Mock PlatformIO's ProjectConfig cache_dir to return whitespace
    with patch(
        "platformio.project.config.ProjectConfig.get_instance"
    ) as mock_get_instance:
        mock_config = MagicMock()
        mock_get_instance.return_value = mock_config
        mock_config.get.side_effect = (
            lambda section, option: "   "  # Whitespace only
            if (section, option) == ("platformio", "cache_dir")
            else ""
        )

        # Call the function
        with caplog.at_level("INFO"):
            clean_build()

    # Verify pioenvs was removed
    assert not pioenvs_dir.exists()

    # Verify no cache cleaning was attempted due to empty string
    assert "PlatformIO cache" not in caplog.text


@patch("esphome.writer.CORE")
def test_write_gitignore_creates_new_file(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test write_gitignore creates a new .gitignore file when it doesn't exist."""
    gitignore_path = tmp_path / ".gitignore"

    # Setup mocks
    mock_core.relative_config_path.return_value = gitignore_path

    # Verify file doesn't exist
    assert not gitignore_path.exists()

    # Call the function
    write_gitignore()

    # Verify file was created with correct content
    assert gitignore_path.exists()
    assert gitignore_path.read_text() == GITIGNORE_CONTENT


@patch("esphome.writer.CORE")
def test_write_gitignore_skips_existing_file(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test write_gitignore doesn't overwrite existing .gitignore file."""
    gitignore_path = tmp_path / ".gitignore"
    existing_content = "# Custom gitignore\n/custom_dir/\n"
    gitignore_path.write_text(existing_content)

    # Setup mocks
    mock_core.relative_config_path.return_value = gitignore_path

    # Verify file exists with custom content
    assert gitignore_path.exists()
    assert gitignore_path.read_text() == existing_content

    # Call the function
    write_gitignore()

    # Verify file was not modified
    assert gitignore_path.exists()
    assert gitignore_path.read_text() == existing_content


@patch("esphome.writer.write_file_if_changed")  # Mock to capture output
@patch("esphome.writer.copy_src_tree")  # Keep this mock as it's complex
@patch("esphome.writer.CORE")
def test_write_cpp_with_existing_file(
    mock_core: MagicMock,
    mock_copy_src_tree: MagicMock,
    mock_write_file: MagicMock,
    tmp_path: Path,
) -> None:
    """Test write_cpp when main.cpp already exists."""
    # Create a real file with markers
    main_cpp = tmp_path / "main.cpp"
    existing_content = f"""#include "esphome.h"
{CPP_INCLUDE_BEGIN}
// Old includes
{CPP_INCLUDE_END}
void setup() {{
{CPP_AUTO_GENERATE_BEGIN}
// Old code
{CPP_AUTO_GENERATE_END}
}}
void loop() {{}}"""
    main_cpp.write_text(existing_content)

    # Setup mocks
    mock_core.relative_src_path.return_value = main_cpp
    mock_core.cpp_global_section = "// Global section"

    # Call the function
    test_code = "  // New generated code"
    write_cpp(test_code)

    # Verify copy_src_tree was called
    mock_copy_src_tree.assert_called_once()

    # Get the content that would be written
    mock_write_file.assert_called_once()
    written_path, written_content = mock_write_file.call_args[0]

    # Check that markers are preserved and content is updated
    assert CPP_INCLUDE_BEGIN in written_content
    assert CPP_INCLUDE_END in written_content
    assert CPP_AUTO_GENERATE_BEGIN in written_content
    assert CPP_AUTO_GENERATE_END in written_content
    assert test_code in written_content
    assert "// Global section" in written_content


@patch("esphome.writer.write_file_if_changed")  # Mock to capture output
@patch("esphome.writer.copy_src_tree")  # Keep this mock as it's complex
@patch("esphome.writer.CORE")
def test_write_cpp_creates_new_file(
    mock_core: MagicMock,
    mock_copy_src_tree: MagicMock,
    mock_write_file: MagicMock,
    tmp_path: Path,
) -> None:
    """Test write_cpp when main.cpp doesn't exist."""
    # Setup path for new file
    main_cpp = tmp_path / "main.cpp"

    # Setup mocks
    mock_core.relative_src_path.return_value = main_cpp
    mock_core.cpp_global_section = "// Global section"

    # Verify file doesn't exist
    assert not main_cpp.exists()

    # Call the function
    test_code = "  // Generated code"
    write_cpp(test_code)

    # Verify copy_src_tree was called
    mock_copy_src_tree.assert_called_once()

    # Get the content that would be written
    mock_write_file.assert_called_once()
    written_path, written_content = mock_write_file.call_args[0]
    assert written_path == main_cpp

    # Check that all necessary parts are in the new file
    assert '#include "esphome.h"' in written_content
    assert CPP_INCLUDE_BEGIN in written_content
    assert CPP_INCLUDE_END in written_content
    assert CPP_AUTO_GENERATE_BEGIN in written_content
    assert CPP_AUTO_GENERATE_END in written_content
    assert test_code in written_content
    assert "void setup()" in written_content
    assert "void loop()" in written_content
    assert "App.setup();" in written_content
    assert "App.loop();" in written_content


@pytest.mark.usefixtures("mock_copy_src_tree")
@patch("esphome.writer.CORE")
def test_write_cpp_with_missing_end_marker(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test write_cpp raises error when end marker is missing."""
    # Create a file with begin marker but no end marker
    main_cpp = tmp_path / "main.cpp"
    existing_content = f"""#include "esphome.h"
{CPP_AUTO_GENERATE_BEGIN}
// Code without end marker"""
    main_cpp.write_text(existing_content)

    # Setup mocks
    mock_core.relative_src_path.return_value = main_cpp

    # Call should raise an error
    with pytest.raises(EsphomeError, match="Could not find auto generated code end"):
        write_cpp("// New code")


@pytest.mark.usefixtures("mock_copy_src_tree")
@patch("esphome.writer.CORE")
def test_write_cpp_with_duplicate_markers(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test write_cpp raises error when duplicate markers exist."""
    # Create a file with duplicate begin markers
    main_cpp = tmp_path / "main.cpp"
    existing_content = f"""#include "esphome.h"
{CPP_AUTO_GENERATE_BEGIN}
// First section
{CPP_AUTO_GENERATE_END}
{CPP_AUTO_GENERATE_BEGIN}
// Duplicate section
{CPP_AUTO_GENERATE_END}"""
    main_cpp.write_text(existing_content)

    # Setup mocks
    mock_core.relative_src_path.return_value = main_cpp

    # Call should raise an error
    with pytest.raises(EsphomeError, match="Found multiple auto generate code begins"):
        write_cpp("// New code")


@patch("esphome.writer.CORE")
def test_clean_all_with_yaml_file(
    mock_core: MagicMock,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test clean_all with a .yaml file uses parent directory."""
    # Create config directory with yaml file
    config_dir = tmp_path / "config"
    config_dir.mkdir()
    yaml_file = config_dir / "test.yaml"
    yaml_file.write_text("esphome:\n  name: test\n")

    build_dir = config_dir / ".esphome"
    build_dir.mkdir()
    (build_dir / "dummy.txt").write_text("x")

    from esphome.writer import clean_all

    with caplog.at_level("INFO"):
        clean_all([str(yaml_file)])

    # Verify .esphome directory still exists but contents cleaned
    assert build_dir.exists()
    assert not (build_dir / "dummy.txt").exists()

    # Verify logging mentions the build dir
    assert "Cleaning" in caplog.text
    assert str(build_dir) in caplog.text


@patch("esphome.writer.CORE")
def test_clean_all(
    mock_core: MagicMock,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test clean_all removes build and PlatformIO dirs."""
    # Create build directories for multiple configurations
    config1_dir = tmp_path / "config1"
    config2_dir = tmp_path / "config2"
    config1_dir.mkdir()
    config2_dir.mkdir()

    build_dir1 = config1_dir / ".esphome"
    build_dir2 = config2_dir / ".esphome"
    build_dir1.mkdir()
    build_dir2.mkdir()
    (build_dir1 / "dummy.txt").write_text("x")
    (build_dir2 / "dummy.txt").write_text("x")

    # Create PlatformIO directories
    pio_cache = tmp_path / "pio_cache"
    pio_packages = tmp_path / "pio_packages"
    pio_platforms = tmp_path / "pio_platforms"
    pio_core = tmp_path / "pio_core"
    for d in (pio_cache, pio_packages, pio_platforms, pio_core):
        d.mkdir()
        (d / "keep").write_text("x")

    # Mock ProjectConfig
    with patch(
        "platformio.project.config.ProjectConfig.get_instance"
    ) as mock_get_instance:
        mock_config = MagicMock()
        mock_get_instance.return_value = mock_config

        def cfg_get(section: str, option: str) -> str:
            mapping = {
                ("platformio", "cache_dir"): str(pio_cache),
                ("platformio", "packages_dir"): str(pio_packages),
                ("platformio", "platforms_dir"): str(pio_platforms),
                ("platformio", "core_dir"): str(pio_core),
            }
            return mapping.get((section, option), "")

        mock_config.get.side_effect = cfg_get

        # Call
        from esphome.writer import clean_all

        with caplog.at_level("INFO"):
            clean_all([str(config1_dir), str(config2_dir)])

    # Verify deletions - .esphome directories remain but contents are cleaned
    # The .esphome directory itself is not removed because it may contain storage
    assert build_dir1.exists()
    assert build_dir2.exists()

    # Verify that files in .esphome were removed
    assert not (build_dir1 / "dummy.txt").exists()
    assert not (build_dir2 / "dummy.txt").exists()
    assert not pio_cache.exists()
    assert not pio_packages.exists()
    assert not pio_platforms.exists()
    assert not pio_core.exists()

    # Verify logging mentions each
    assert "Cleaning" in caplog.text
    assert str(build_dir1) in caplog.text
    assert str(build_dir2) in caplog.text
    assert "PlatformIO cache" in caplog.text
    assert "PlatformIO packages" in caplog.text
    assert "PlatformIO platforms" in caplog.text
    assert "PlatformIO core" in caplog.text


@patch("esphome.writer.CORE")
def test_clean_all_preserves_storage(
    mock_core: MagicMock,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test clean_all preserves storage directory."""
    # Create build directory with storage subdirectory
    config_dir = tmp_path / "config"
    config_dir.mkdir()

    build_dir = config_dir / ".esphome"
    build_dir.mkdir()
    (build_dir / "dummy.txt").write_text("x")
    (build_dir / "other_file.txt").write_text("y")

    # Create storage directory with content
    storage_dir = build_dir / "storage"
    storage_dir.mkdir()
    (storage_dir / "storage.json").write_text('{"test": "data"}')
    (storage_dir / "other_storage.txt").write_text("storage content")

    # Call clean_all
    from esphome.writer import clean_all

    with caplog.at_level("INFO"):
        clean_all([str(config_dir)])

    # Verify .esphome directory still exists
    assert build_dir.exists()

    # Verify storage directory still exists with its contents
    assert storage_dir.exists()
    assert (storage_dir / "storage.json").exists()
    assert (storage_dir / "other_storage.txt").exists()

    # Verify storage contents are intact
    assert (storage_dir / "storage.json").read_text() == '{"test": "data"}'
    assert (storage_dir / "other_storage.txt").read_text() == "storage content"

    # Verify other files were removed
    assert not (build_dir / "dummy.txt").exists()
    assert not (build_dir / "other_file.txt").exists()

    # Verify logging mentions deletion
    assert "Cleaning" in caplog.text
    assert str(build_dir) in caplog.text


@patch("esphome.writer.CORE")
def test_clean_all_platformio_not_available(
    mock_core: MagicMock,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test clean_all when PlatformIO is not available."""
    # Build dirs
    config_dir = tmp_path / "config"
    config_dir.mkdir()
    build_dir = config_dir / ".esphome"
    build_dir.mkdir()

    # PlatformIO dirs that should remain untouched
    pio_cache = tmp_path / "pio_cache"
    pio_cache.mkdir()

    from esphome.writer import clean_all

    with (
        patch.dict("sys.modules", {"platformio.project.config": None}),
        caplog.at_level("INFO"),
    ):
        clean_all([str(config_dir)])

        # Build dir contents cleaned, PlatformIO dirs remain
        assert build_dir.exists()
    assert pio_cache.exists()

    # No PlatformIO-specific logs
    assert "PlatformIO" not in caplog.text


@patch("esphome.writer.CORE")
def test_clean_all_partial_exists(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test clean_all when only some build dirs exist."""
    config_dir = tmp_path / "config"
    config_dir.mkdir()
    build_dir = config_dir / ".esphome"
    build_dir.mkdir()

    with patch(
        "platformio.project.config.ProjectConfig.get_instance"
    ) as mock_get_instance:
        mock_config = MagicMock()
        mock_get_instance.return_value = mock_config
        # Return non-existent dirs
        mock_config.get.side_effect = lambda *_args, **_kw: str(
            tmp_path / "does_not_exist"
        )

        from esphome.writer import clean_all

        clean_all([str(config_dir)])

        assert build_dir.exists()


@patch("esphome.writer.CORE")
def test_clean_all_removes_non_storage_directories(
    mock_core: MagicMock,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test clean_all removes directories other than storage."""
    # Create build directory with various subdirectories
    config_dir = tmp_path / "config"
    config_dir.mkdir()

    build_dir = config_dir / ".esphome"
    build_dir.mkdir()

    # Create files
    (build_dir / "file1.txt").write_text("content1")
    (build_dir / "file2.txt").write_text("content2")

    # Create storage directory (should be preserved)
    storage_dir = build_dir / "storage"
    storage_dir.mkdir()
    (storage_dir / "storage.json").write_text('{"test": "data"}')

    # Create other directories (should be removed)
    cache_dir = build_dir / "cache"
    cache_dir.mkdir()
    (cache_dir / "cache_file.txt").write_text("cache content")

    logs_dir = build_dir / "logs"
    logs_dir.mkdir()
    (logs_dir / "log1.txt").write_text("log content")

    temp_dir = build_dir / "temp"
    temp_dir.mkdir()
    (temp_dir / "temp_file.txt").write_text("temp content")

    # Call clean_all
    from esphome.writer import clean_all

    with caplog.at_level("INFO"):
        clean_all([str(config_dir)])

    # Verify .esphome directory still exists
    assert build_dir.exists()

    # Verify storage directory and its contents are preserved
    assert storage_dir.exists()
    assert (storage_dir / "storage.json").exists()
    assert (storage_dir / "storage.json").read_text() == '{"test": "data"}'

    # Verify files were removed
    assert not (build_dir / "file1.txt").exists()
    assert not (build_dir / "file2.txt").exists()

    # Verify non-storage directories were removed
    assert not cache_dir.exists()
    assert not logs_dir.exists()
    assert not temp_dir.exists()

    # Verify logging mentions cleaning
    assert "Cleaning" in caplog.text
    assert str(build_dir) in caplog.text


@patch("esphome.writer.CORE")
def test_clean_all_preserves_json_files(
    mock_core: MagicMock,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test clean_all preserves .json files."""
    # Create build directory with various files
    config_dir = tmp_path / "config"
    config_dir.mkdir()

    build_dir = config_dir / ".esphome"
    build_dir.mkdir()

    # Create .json files (should be preserved)
    (build_dir / "config.json").write_text('{"config": "data"}')
    (build_dir / "metadata.json").write_text('{"metadata": "info"}')

    # Create non-.json files (should be removed)
    (build_dir / "dummy.txt").write_text("x")
    (build_dir / "other.log").write_text("log content")

    # Call clean_all
    from esphome.writer import clean_all

    with caplog.at_level("INFO"):
        clean_all([str(config_dir)])

    # Verify .esphome directory still exists
    assert build_dir.exists()

    # Verify .json files are preserved
    assert (build_dir / "config.json").exists()
    assert (build_dir / "config.json").read_text() == '{"config": "data"}'
    assert (build_dir / "metadata.json").exists()
    assert (build_dir / "metadata.json").read_text() == '{"metadata": "info"}'

    # Verify non-.json files were removed
    assert not (build_dir / "dummy.txt").exists()
    assert not (build_dir / "other.log").exists()

    # Verify logging mentions cleaning
    assert "Cleaning" in caplog.text
    assert str(build_dir) in caplog.text


@patch("esphome.writer.CORE")
def test_clean_build_handles_readonly_files(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test clean_build handles read-only files (e.g., git pack files on Windows)."""
    # Create directory structure with read-only files
    pioenvs_dir = tmp_path / ".pioenvs"
    pioenvs_dir.mkdir()
    git_dir = pioenvs_dir / ".git" / "objects" / "pack"
    git_dir.mkdir(parents=True)

    # Create a read-only file (simulating git pack files on Windows)
    readonly_file = git_dir / "pack-abc123.pack"
    readonly_file.write_text("pack data")
    os.chmod(readonly_file, stat.S_IRUSR)  # Read-only

    # Setup mocks
    mock_core.relative_pioenvs_path.return_value = pioenvs_dir
    mock_core.relative_piolibdeps_path.return_value = tmp_path / ".piolibdeps"
    mock_core.relative_build_path.return_value = tmp_path / "dependencies.lock"

    # Verify file is read-only
    assert not os.access(readonly_file, os.W_OK)

    # Call the function - should not crash
    clean_build()

    # Verify directory was removed despite read-only files
    assert not pioenvs_dir.exists()


@patch("esphome.writer.CORE")
def test_clean_all_handles_readonly_files(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test clean_all handles read-only files."""
    # Create config directory
    config_dir = tmp_path / "config"
    config_dir.mkdir()

    build_dir = config_dir / ".esphome"
    build_dir.mkdir()

    # Create a subdirectory with read-only files
    subdir = build_dir / "subdir"
    subdir.mkdir()
    readonly_file = subdir / "readonly.txt"
    readonly_file.write_text("content")
    os.chmod(readonly_file, stat.S_IRUSR)  # Read-only

    # Verify file is read-only
    assert not os.access(readonly_file, os.W_OK)

    # Call the function - should not crash
    clean_all([str(config_dir)])

    # Verify directory was removed despite read-only files
    assert not subdir.exists()
    assert build_dir.exists()  # .esphome dir itself is preserved


@patch("esphome.writer.CORE")
def test_clean_build_reraises_for_other_errors(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test clean_build re-raises errors that are not read-only permission issues."""
    # Create directory structure with a read-only subdirectory
    # This prevents file deletion and triggers the error handler
    pioenvs_dir = tmp_path / ".pioenvs"
    pioenvs_dir.mkdir()
    subdir = pioenvs_dir / "subdir"
    subdir.mkdir()
    test_file = subdir / "test.txt"
    test_file.write_text("content")

    # Make subdir read-only so files inside can't be deleted
    os.chmod(subdir, stat.S_IRUSR | stat.S_IXUSR)

    # Setup mocks
    mock_core.relative_pioenvs_path.return_value = pioenvs_dir
    mock_core.relative_piolibdeps_path.return_value = tmp_path / ".piolibdeps"
    mock_core.relative_build_path.return_value = tmp_path / "dependencies.lock"

    try:
        # Mock os.access in writer module to return True (writable)
        # This simulates a case where the error is NOT due to read-only permissions
        # so the error handler should re-raise instead of trying to fix permissions
        with (
            patch("esphome.writer.os.access", return_value=True),
            pytest.raises(PermissionError),
        ):
            clean_build()
    finally:
        # Cleanup - restore write permission so tmp_path cleanup works
        os.chmod(subdir, stat.S_IRWXU)


# Tests for get_build_info()


@patch("esphome.writer.CORE")
def test_get_build_info_new_build(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test get_build_info returns new build_time when no existing build_info.json."""
    build_info_path = tmp_path / "build_info.json"
    mock_core.relative_build_path.return_value = build_info_path
    mock_core.config_hash = 0x12345678
    mock_core.comment = "Test comment"

    config_hash, build_time, build_time_str, comment = get_build_info()

    assert config_hash == 0x12345678
    assert isinstance(build_time, int)
    assert build_time > 0
    assert isinstance(build_time_str, str)
    # Verify build_time_str format matches expected pattern
    assert len(build_time_str) >= 19  # e.g., "2025-12-15 16:27:44 +0000"
    assert comment == "Test comment"


@patch("esphome.writer.CORE")
def test_get_build_info_always_returns_current_time(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test get_build_info always returns current build_time."""
    build_info_path = tmp_path / "build_info.json"
    mock_core.relative_build_path.return_value = build_info_path
    mock_core.config_hash = 0x12345678
    mock_core.comment = ""

    # Create existing build_info.json with matching config_hash and version
    existing_build_time = 1700000000
    existing_build_time_str = "2023-11-14 22:13:20 +0000"
    build_info_path.write_text(
        json.dumps(
            {
                "config_hash": 0x12345678,
                "build_time": existing_build_time,
                "build_time_str": existing_build_time_str,
                "esphome_version": "2025.1.0-dev",
            }
        )
    )

    with patch("esphome.writer.__version__", "2025.1.0-dev"):
        config_hash, build_time, build_time_str, comment = get_build_info()

    assert config_hash == 0x12345678
    # get_build_info now always returns current time
    assert build_time != existing_build_time
    assert build_time > existing_build_time
    assert build_time_str != existing_build_time_str


@patch("esphome.writer.CORE")
def test_get_build_info_config_changed(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test get_build_info returns new build_time when config hash changed."""
    build_info_path = tmp_path / "build_info.json"
    mock_core.relative_build_path.return_value = build_info_path
    mock_core.config_hash = 0xABCDEF00  # Different from existing
    mock_core.comment = ""

    # Create existing build_info.json with different config_hash
    existing_build_time = 1700000000
    build_info_path.write_text(
        json.dumps(
            {
                "config_hash": 0x12345678,  # Different
                "build_time": existing_build_time,
                "build_time_str": "2023-11-14 22:13:20 +0000",
                "esphome_version": "2025.1.0-dev",
            }
        )
    )

    with patch("esphome.writer.__version__", "2025.1.0-dev"):
        config_hash, build_time, build_time_str, comment = get_build_info()

    assert config_hash == 0xABCDEF00
    assert build_time != existing_build_time  # New time generated
    assert build_time > existing_build_time


@patch("esphome.writer.CORE")
def test_get_build_info_version_changed(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test get_build_info returns new build_time when ESPHome version changed."""
    build_info_path = tmp_path / "build_info.json"
    mock_core.relative_build_path.return_value = build_info_path
    mock_core.config_hash = 0x12345678
    mock_core.comment = ""

    # Create existing build_info.json with different version
    existing_build_time = 1700000000
    build_info_path.write_text(
        json.dumps(
            {
                "config_hash": 0x12345678,
                "build_time": existing_build_time,
                "build_time_str": "2023-11-14 22:13:20 +0000",
                "esphome_version": "2024.12.0",  # Old version
            }
        )
    )

    with patch("esphome.writer.__version__", "2025.1.0-dev"):  # New version
        config_hash, build_time, build_time_str, comment = get_build_info()

    assert config_hash == 0x12345678
    assert build_time != existing_build_time  # New time generated
    assert build_time > existing_build_time


@patch("esphome.writer.CORE")
def test_get_build_info_invalid_json(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test get_build_info handles invalid JSON gracefully."""
    build_info_path = tmp_path / "build_info.json"
    mock_core.relative_build_path.return_value = build_info_path
    mock_core.config_hash = 0x12345678
    mock_core.comment = ""

    # Create invalid JSON file
    build_info_path.write_text("not valid json {{{")

    config_hash, build_time, build_time_str, comment = get_build_info()

    assert config_hash == 0x12345678
    assert isinstance(build_time, int)
    assert build_time > 0


@patch("esphome.writer.CORE")
def test_get_build_info_missing_keys(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test get_build_info handles missing keys gracefully."""
    build_info_path = tmp_path / "build_info.json"
    mock_core.relative_build_path.return_value = build_info_path
    mock_core.config_hash = 0x12345678
    mock_core.comment = ""

    # Create JSON with missing keys
    build_info_path.write_text(json.dumps({"config_hash": 0x12345678}))

    with patch("esphome.writer.__version__", "2025.1.0-dev"):
        config_hash, build_time, build_time_str, comment = get_build_info()

    assert config_hash == 0x12345678
    assert isinstance(build_time, int)
    assert build_time > 0


@patch("esphome.writer.CORE")
def test_get_build_info_build_time_str_format(
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test get_build_info returns correctly formatted build_time_str."""
    build_info_path = tmp_path / "build_info.json"
    mock_core.relative_build_path.return_value = build_info_path
    mock_core.config_hash = 0x12345678
    mock_core.comment = ""

    config_hash, build_time, build_time_str, comment = get_build_info()

    # Verify the format matches "%Y-%m-%d %H:%M:%S %z"
    # e.g., "2025-12-15 16:27:44 +0000"
    parsed = datetime.strptime(build_time_str, "%Y-%m-%d %H:%M:%S %z")
    assert parsed.year >= 2024


def test_generate_build_info_data_h_format() -> None:
    """Test generate_build_info_data_h produces correct header content."""
    config_hash = 0x12345678
    build_time = 1700000000
    build_time_str = "2023-11-14 22:13:20 +0000"
    comment = "Test comment"

    result = generate_build_info_data_h(
        config_hash, build_time, build_time_str, comment
    )

    assert "#pragma once" in result
    assert "#define ESPHOME_CONFIG_HASH 0x12345678U" in result
    assert "#define ESPHOME_BUILD_TIME 1700000000" in result
    assert "#define ESPHOME_COMMENT_SIZE 13" in result  # len("Test comment") + 1
    assert 'ESPHOME_BUILD_TIME_STR[] = "2023-11-14 22:13:20 +0000"' in result
    assert 'ESPHOME_COMMENT_STR[] = "Test comment"' in result


def test_generate_build_info_data_h_esp8266_progmem() -> None:
    """Test generate_build_info_data_h includes PROGMEM for ESP8266."""
    result = generate_build_info_data_h(0xABCDEF01, 1700000000, "test", "comment")

    # Should have ESP8266 PROGMEM conditional
    assert "#ifdef USE_ESP8266" in result
    assert "#include <pgmspace.h>" in result
    assert "PROGMEM" in result
    # Both build time and comment should have PROGMEM versions
    assert 'ESPHOME_BUILD_TIME_STR[] PROGMEM = "test"' in result
    assert 'ESPHOME_COMMENT_STR[] PROGMEM = "comment"' in result


def test_generate_build_info_data_h_hash_formatting() -> None:
    """Test generate_build_info_data_h formats hash with leading zeros."""
    # Test with small hash value that needs leading zeros
    result = generate_build_info_data_h(0x00000001, 0, "test", "")
    assert "#define ESPHOME_CONFIG_HASH 0x00000001U" in result

    # Test with larger hash value
    result = generate_build_info_data_h(0xFFFFFFFF, 0, "test", "")
    assert "#define ESPHOME_CONFIG_HASH 0xffffffffU" in result


def test_generate_build_info_data_h_comment_escaping() -> None:
    r"""Test generate_build_info_data_h properly escapes special characters in comment.

    Uses cpp_string_escape which outputs octal escapes for special characters:
    - backslash (ASCII 92) -> \134
    - double quote (ASCII 34) -> \042
    - newline (ASCII 10) -> \012
    """
    # Test backslash escaping (ASCII 92 = octal 134)
    result = generate_build_info_data_h(0, 0, "test", "backslash\\here")
    assert 'ESPHOME_COMMENT_STR[] = "backslash\\134here"' in result

    # Test quote escaping (ASCII 34 = octal 042)
    result = generate_build_info_data_h(0, 0, "test", 'has "quotes"')
    assert 'ESPHOME_COMMENT_STR[] = "has \\042quotes\\042"' in result

    # Test newline escaping (ASCII 10 = octal 012)
    result = generate_build_info_data_h(0, 0, "test", "line1\nline2")
    assert 'ESPHOME_COMMENT_STR[] = "line1\\012line2"' in result


def test_generate_build_info_data_h_empty_comment() -> None:
    """Test generate_build_info_data_h handles empty comment."""
    result = generate_build_info_data_h(0, 0, "test", "")

    assert "#define ESPHOME_COMMENT_SIZE 1" in result  # Just null terminator
    assert 'ESPHOME_COMMENT_STR[] = ""' in result


@patch("esphome.writer.CORE")
@patch("esphome.writer.iter_components")
@patch("esphome.writer.walk_files")
def test_copy_src_tree_writes_build_info_files(
    mock_walk_files: MagicMock,
    mock_iter_components: MagicMock,
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test copy_src_tree writes build_info_data.h and build_info.json."""
    # Setup directory structure
    src_path = tmp_path / "src"
    src_path.mkdir()
    esphome_core_path = src_path / "esphome" / "core"
    esphome_core_path.mkdir(parents=True)
    build_path = tmp_path / "build"
    build_path.mkdir()

    # Create mock source files for defines.h and version.h
    mock_defines_h = esphome_core_path / "defines.h"
    mock_defines_h.write_text("// mock defines.h")
    mock_version_h = esphome_core_path / "version.h"
    mock_version_h.write_text("// mock version.h")

    # Create mock FileResource that returns our temp files
    @dataclass(frozen=True)
    class MockFileResource:
        package: str
        resource: str
        _path: Path

        @contextmanager
        def path(self):
            yield self._path

    # Create mock resources for defines.h and version.h (required by copy_src_tree)
    mock_resources = [
        MockFileResource(
            package="esphome.core",
            resource="defines.h",
            _path=mock_defines_h,
        ),
        MockFileResource(
            package="esphome.core",
            resource="version.h",
            _path=mock_version_h,
        ),
    ]

    # Create mock component with resources
    mock_component = MagicMock()
    mock_component.resources = mock_resources

    # Setup mocks
    mock_core.relative_src_path.side_effect = lambda *args: src_path.joinpath(*args)
    mock_core.relative_build_path.side_effect = lambda *args: build_path.joinpath(*args)
    mock_core.defines = []
    mock_core.config_hash = 0xDEADBEEF
    mock_core.comment = "Test comment"
    mock_core.target_platform = "test_platform"
    mock_core.config = {}
    mock_iter_components.return_value = [("core", mock_component)]
    mock_walk_files.return_value = []

    # Create mock module without copy_files attribute (causes AttributeError which is caught)
    mock_module = MagicMock(spec=[])  # Empty spec = no copy_files attribute

    with (
        patch("esphome.writer.__version__", "2025.1.0-dev"),
        patch("esphome.writer.importlib.import_module", return_value=mock_module),
    ):
        copy_src_tree()

    # Verify build_info_data.h was written
    build_info_h_path = esphome_core_path / "build_info_data.h"
    assert build_info_h_path.exists()
    build_info_h_content = build_info_h_path.read_text()
    assert "#define ESPHOME_CONFIG_HASH 0xdeadbeefU" in build_info_h_content
    assert "#define ESPHOME_BUILD_TIME" in build_info_h_content
    assert "ESPHOME_BUILD_TIME_STR" in build_info_h_content
    assert "#define ESPHOME_COMMENT_SIZE" in build_info_h_content
    assert "ESPHOME_COMMENT_STR" in build_info_h_content

    # Verify build_info.json was written
    build_info_json_path = build_path / "build_info.json"
    assert build_info_json_path.exists()
    build_info_json = json.loads(build_info_json_path.read_text())
    assert build_info_json["config_hash"] == 0xDEADBEEF
    assert "build_time" in build_info_json
    assert "build_time_str" in build_info_json
    assert build_info_json["esphome_version"] == "2025.1.0-dev"


@patch("esphome.writer.CORE")
@patch("esphome.writer.iter_components")
@patch("esphome.writer.walk_files")
def test_copy_src_tree_detects_config_hash_change(
    mock_walk_files: MagicMock,
    mock_iter_components: MagicMock,
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test copy_src_tree detects when config_hash changes."""
    # Setup directory structure
    src_path = tmp_path / "src"
    src_path.mkdir()
    esphome_core_path = src_path / "esphome" / "core"
    esphome_core_path.mkdir(parents=True)
    build_path = tmp_path / "build"
    build_path.mkdir()

    # Create existing build_info.json with different config_hash
    build_info_json_path = build_path / "build_info.json"
    build_info_json_path.write_text(
        json.dumps(
            {
                "config_hash": 0x12345678,  # Different from current
                "build_time": 1700000000,
                "build_time_str": "2023-11-14 22:13:20 +0000",
                "esphome_version": "2025.1.0-dev",
            }
        )
    )

    # Create existing build_info_data.h
    build_info_h_path = esphome_core_path / "build_info_data.h"
    build_info_h_path.write_text("// old build_info_data.h")

    # Setup mocks
    mock_core.relative_src_path.side_effect = lambda *args: src_path.joinpath(*args)
    mock_core.relative_build_path.side_effect = lambda *args: build_path.joinpath(*args)
    mock_core.defines = []
    mock_core.config_hash = 0xDEADBEEF  # Different from existing
    mock_core.comment = ""
    mock_core.target_platform = "test_platform"
    mock_core.config = {}
    mock_iter_components.return_value = []
    mock_walk_files.return_value = []

    with (
        patch("esphome.writer.__version__", "2025.1.0-dev"),
        patch("esphome.writer.importlib.import_module") as mock_import,
    ):
        mock_import.side_effect = AttributeError
        copy_src_tree()

    # Verify build_info files were updated due to config_hash change
    assert build_info_h_path.exists()
    new_content = build_info_h_path.read_text()
    assert "0xdeadbeef" in new_content.lower()

    new_json = json.loads(build_info_json_path.read_text())
    assert new_json["config_hash"] == 0xDEADBEEF


@patch("esphome.writer.CORE")
@patch("esphome.writer.iter_components")
@patch("esphome.writer.walk_files")
def test_copy_src_tree_detects_version_change(
    mock_walk_files: MagicMock,
    mock_iter_components: MagicMock,
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test copy_src_tree detects when esphome_version changes."""
    # Setup directory structure
    src_path = tmp_path / "src"
    src_path.mkdir()
    esphome_core_path = src_path / "esphome" / "core"
    esphome_core_path.mkdir(parents=True)
    build_path = tmp_path / "build"
    build_path.mkdir()

    # Create existing build_info.json with different version
    build_info_json_path = build_path / "build_info.json"
    build_info_json_path.write_text(
        json.dumps(
            {
                "config_hash": 0xDEADBEEF,
                "build_time": 1700000000,
                "build_time_str": "2023-11-14 22:13:20 +0000",
                "esphome_version": "2024.12.0",  # Old version
            }
        )
    )

    # Create existing build_info_data.h
    build_info_h_path = esphome_core_path / "build_info_data.h"
    build_info_h_path.write_text("// old build_info_data.h")

    # Setup mocks
    mock_core.relative_src_path.side_effect = lambda *args: src_path.joinpath(*args)
    mock_core.relative_build_path.side_effect = lambda *args: build_path.joinpath(*args)
    mock_core.defines = []
    mock_core.config_hash = 0xDEADBEEF
    mock_core.comment = ""
    mock_core.target_platform = "test_platform"
    mock_core.config = {}
    mock_iter_components.return_value = []
    mock_walk_files.return_value = []

    with (
        patch("esphome.writer.__version__", "2025.1.0-dev"),  # New version
        patch("esphome.writer.importlib.import_module") as mock_import,
    ):
        mock_import.side_effect = AttributeError
        copy_src_tree()

    # Verify build_info files were updated due to version change
    assert build_info_h_path.exists()
    new_json = json.loads(build_info_json_path.read_text())
    assert new_json["esphome_version"] == "2025.1.0-dev"


@patch("esphome.writer.CORE")
@patch("esphome.writer.iter_components")
@patch("esphome.writer.walk_files")
def test_copy_src_tree_handles_invalid_build_info_json(
    mock_walk_files: MagicMock,
    mock_iter_components: MagicMock,
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test copy_src_tree handles invalid build_info.json gracefully."""
    # Setup directory structure
    src_path = tmp_path / "src"
    src_path.mkdir()
    esphome_core_path = src_path / "esphome" / "core"
    esphome_core_path.mkdir(parents=True)
    build_path = tmp_path / "build"
    build_path.mkdir()

    # Create invalid build_info.json
    build_info_json_path = build_path / "build_info.json"
    build_info_json_path.write_text("invalid json {{{")

    # Create existing build_info_data.h
    build_info_h_path = esphome_core_path / "build_info_data.h"
    build_info_h_path.write_text("// old build_info_data.h")

    # Setup mocks
    mock_core.relative_src_path.side_effect = lambda *args: src_path.joinpath(*args)
    mock_core.relative_build_path.side_effect = lambda *args: build_path.joinpath(*args)
    mock_core.defines = []
    mock_core.config_hash = 0xDEADBEEF
    mock_core.comment = ""
    mock_core.target_platform = "test_platform"
    mock_core.config = {}
    mock_iter_components.return_value = []
    mock_walk_files.return_value = []

    with (
        patch("esphome.writer.__version__", "2025.1.0-dev"),
        patch("esphome.writer.importlib.import_module") as mock_import,
    ):
        mock_import.side_effect = AttributeError
        copy_src_tree()

    # Verify build_info files were created despite invalid JSON
    assert build_info_h_path.exists()
    new_json = json.loads(build_info_json_path.read_text())
    assert new_json["config_hash"] == 0xDEADBEEF


@patch("esphome.writer.CORE")
@patch("esphome.writer.iter_components")
@patch("esphome.writer.walk_files")
def test_copy_src_tree_build_info_timestamp_behavior(
    mock_walk_files: MagicMock,
    mock_iter_components: MagicMock,
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test build_info behaviour: regenerated on change, preserved when unchanged."""
    # Setup directory structure
    src_path = tmp_path / "src"
    src_path.mkdir()
    esphome_core_path = src_path / "esphome" / "core"
    esphome_core_path.mkdir(parents=True)
    esphome_components_path = src_path / "esphome" / "components"
    esphome_components_path.mkdir(parents=True)
    build_path = tmp_path / "build"
    build_path.mkdir()

    # Create a source file
    source_file = tmp_path / "source" / "test.cpp"
    source_file.parent.mkdir()
    source_file.write_text("// version 1")

    # Create destination file in build tree
    dest_file = esphome_components_path / "test.cpp"

    # Create mock FileResource
    @dataclass(frozen=True)
    class MockFileResource:
        package: str
        resource: str
        _path: Path

        @contextmanager
        def path(self):
            yield self._path

    mock_resources = [
        MockFileResource(
            package="esphome.components",
            resource="test.cpp",
            _path=source_file,
        ),
    ]

    mock_component = MagicMock()
    mock_component.resources = mock_resources

    # Setup mocks
    mock_core.relative_src_path.side_effect = lambda *args: src_path.joinpath(*args)
    mock_core.relative_build_path.side_effect = lambda *args: build_path.joinpath(*args)
    mock_core.defines = []
    mock_core.config_hash = 0xDEADBEEF
    mock_core.comment = ""
    mock_core.target_platform = "test_platform"
    mock_core.config = {}
    mock_iter_components.return_value = [("test", mock_component)]

    build_info_json_path = build_path / "build_info.json"

    # First run: initial setup, should create build_info
    mock_walk_files.return_value = []
    with (
        patch("esphome.writer.__version__", "2025.1.0-dev"),
        patch("esphome.writer.importlib.import_module") as mock_import,
    ):
        mock_import.side_effect = AttributeError
        copy_src_tree()

    # Manually set an old timestamp for testing
    old_timestamp = 1700000000
    old_timestamp_str = "2023-11-14 22:13:20 +0000"
    build_info_json_path.write_text(
        json.dumps(
            {
                "config_hash": 0xDEADBEEF,
                "build_time": old_timestamp,
                "build_time_str": old_timestamp_str,
                "esphome_version": "2025.1.0-dev",
            }
        )
    )

    # Second run: no changes, should NOT regenerate build_info
    mock_walk_files.return_value = [str(dest_file)]
    with (
        patch("esphome.writer.__version__", "2025.1.0-dev"),
        patch("esphome.writer.importlib.import_module") as mock_import,
    ):
        mock_import.side_effect = AttributeError
        copy_src_tree()

    second_json = json.loads(build_info_json_path.read_text())
    second_timestamp = second_json["build_time"]

    # Verify timestamp was NOT changed
    assert second_timestamp == old_timestamp, (
        f"build_info should not be regenerated when no files change: "
        f"{old_timestamp} != {second_timestamp}"
    )

    # Third run: change source file, should regenerate build_info with new timestamp
    source_file.write_text("// version 2")
    with (
        patch("esphome.writer.__version__", "2025.1.0-dev"),
        patch("esphome.writer.importlib.import_module") as mock_import,
    ):
        mock_import.side_effect = AttributeError
        copy_src_tree()

    third_json = json.loads(build_info_json_path.read_text())
    third_timestamp = third_json["build_time"]

    # Verify timestamp WAS changed
    assert third_timestamp != old_timestamp, (
        f"build_info should be regenerated when source file changes: "
        f"{old_timestamp} == {third_timestamp}"
    )
    assert third_timestamp > old_timestamp


@patch("esphome.writer.CORE")
@patch("esphome.writer.iter_components")
@patch("esphome.writer.walk_files")
def test_copy_src_tree_detects_removed_source_file(
    mock_walk_files: MagicMock,
    mock_iter_components: MagicMock,
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test copy_src_tree detects when a non-generated source file is removed."""
    # Setup directory structure
    src_path = tmp_path / "src"
    src_path.mkdir()
    esphome_components_path = src_path / "esphome" / "components"
    esphome_components_path.mkdir(parents=True)
    build_path = tmp_path / "build"
    build_path.mkdir()

    # Create an existing source file in the build tree
    existing_file = esphome_components_path / "test.cpp"
    existing_file.write_text("// test file")

    # Setup mocks - no components, so the file should be removed
    mock_core.relative_src_path.side_effect = lambda *args: src_path.joinpath(*args)
    mock_core.relative_build_path.side_effect = lambda *args: build_path.joinpath(*args)
    mock_core.defines = []
    mock_core.config_hash = 0xDEADBEEF
    mock_core.comment = ""
    mock_core.target_platform = "test_platform"
    mock_core.config = {}
    mock_iter_components.return_value = []  # No components = file should be removed
    mock_walk_files.return_value = [str(existing_file)]

    # Create existing build_info.json
    build_info_json_path = build_path / "build_info.json"
    old_timestamp = 1700000000
    build_info_json_path.write_text(
        json.dumps(
            {
                "config_hash": 0xDEADBEEF,
                "build_time": old_timestamp,
                "build_time_str": "2023-11-14 22:13:20 +0000",
                "esphome_version": "2025.1.0-dev",
            }
        )
    )

    with (
        patch("esphome.writer.__version__", "2025.1.0-dev"),
        patch("esphome.writer.importlib.import_module") as mock_import,
    ):
        mock_import.side_effect = AttributeError
        copy_src_tree()

    # Verify file was removed
    assert not existing_file.exists()

    # Verify build_info was regenerated due to source file removal
    new_json = json.loads(build_info_json_path.read_text())
    assert new_json["build_time"] != old_timestamp


@patch("esphome.writer.CORE")
@patch("esphome.writer.iter_components")
@patch("esphome.writer.walk_files")
def test_copy_src_tree_ignores_removed_generated_file(
    mock_walk_files: MagicMock,
    mock_iter_components: MagicMock,
    mock_core: MagicMock,
    tmp_path: Path,
) -> None:
    """Test copy_src_tree doesn't mark sources_changed when only generated file removed."""
    # Setup directory structure
    src_path = tmp_path / "src"
    src_path.mkdir()
    esphome_core_path = src_path / "esphome" / "core"
    esphome_core_path.mkdir(parents=True)
    build_path = tmp_path / "build"
    build_path.mkdir()

    # Create existing build_info_data.h (a generated file)
    build_info_h = esphome_core_path / "build_info_data.h"
    build_info_h.write_text("// old generated file")

    # Setup mocks
    mock_core.relative_src_path.side_effect = lambda *args: src_path.joinpath(*args)
    mock_core.relative_build_path.side_effect = lambda *args: build_path.joinpath(*args)
    mock_core.defines = []
    mock_core.config_hash = 0xDEADBEEF
    mock_core.comment = ""
    mock_core.target_platform = "test_platform"
    mock_core.config = {}
    mock_iter_components.return_value = []
    # walk_files returns the generated file, but it's not in source_files_copy
    mock_walk_files.return_value = [str(build_info_h)]

    # Create existing build_info.json with old timestamp
    build_info_json_path = build_path / "build_info.json"
    old_timestamp = 1700000000
    build_info_json_path.write_text(
        json.dumps(
            {
                "config_hash": 0xDEADBEEF,
                "build_time": old_timestamp,
                "build_time_str": "2023-11-14 22:13:20 +0000",
                "esphome_version": "2025.1.0-dev",
            }
        )
    )

    with (
        patch("esphome.writer.__version__", "2025.1.0-dev"),
        patch("esphome.writer.importlib.import_module") as mock_import,
    ):
        mock_import.side_effect = AttributeError
        copy_src_tree()

    # Verify build_info_data.h was regenerated (not removed)
    assert build_info_h.exists()

    # Note: build_info.json will have a new timestamp because get_build_info()
    # always returns current time. The key test is that the old build_info_data.h
    # file was removed and regenerated, not that it triggered sources_changed.
    new_json = json.loads(build_info_json_path.read_text())
    assert new_json["config_hash"] == 0xDEADBEEF
