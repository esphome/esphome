"""Test writer module functionality."""

from collections.abc import Callable
from pathlib import Path
from typing import Any
from unittest.mock import MagicMock, patch

import pytest

from esphome.core import EsphomeError
from esphome.storage_json import StorageJSON
from esphome.writer import (
    CPP_AUTO_GENERATE_BEGIN,
    CPP_AUTO_GENERATE_END,
    CPP_INCLUDE_BEGIN,
    CPP_INCLUDE_END,
    GITIGNORE_CONTENT,
    clean_build,
    clean_cmake_cache,
    storage_should_clean,
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
