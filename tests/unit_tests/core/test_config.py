"""Unit tests for core config functionality including areas and devices."""

from collections.abc import Callable
import os
from pathlib import Path
import types
from typing import Any
from unittest.mock import MagicMock, Mock, patch

import pytest

from esphome import config_validation as cv, core
from esphome.const import (
    CONF_AREA,
    CONF_AREAS,
    CONF_BUILD_PATH,
    CONF_DEVICES,
    CONF_ESPHOME,
    CONF_NAME,
    CONF_NAME_ADD_MAC_SUFFIX,
    KEY_CORE,
)
from esphome.core import CORE, config
from esphome.core.config import (
    Area,
    preload_core_config,
    valid_include,
    valid_project_name,
    validate_area_config,
    validate_hostname,
)

from .common import load_config_from_fixture

FIXTURES_DIR = Path(__file__).parent.parent / "fixtures" / "core" / "config"


def test_validate_area_config_with_string() -> None:
    """Test that string area config is converted to structured format."""
    result = validate_area_config("Living Room")

    assert isinstance(result, dict)
    assert "id" in result
    assert "name" in result
    assert result["name"] == "Living Room"
    assert isinstance(result["id"], core.ID)
    assert result["id"].is_declaration
    assert not result["id"].is_manual


def test_validate_area_config_with_dict() -> None:
    """Test that structured area config passes through unchanged."""
    area_id = cv.declare_id(Area)("test_area")
    input_config: dict[str, Any] = {
        "id": area_id,
        "name": "Test Area",
    }

    result = validate_area_config(input_config)

    assert result == input_config
    assert result["id"] == area_id
    assert result["name"] == "Test Area"


def test_device_with_valid_area_id(yaml_file: Callable[[str], str]) -> None:
    """Test that device with valid area_id works correctly."""
    result = load_config_from_fixture(yaml_file, "valid_area_device.yaml", FIXTURES_DIR)
    assert result is not None

    esphome_config = result["esphome"]

    # Verify areas were parsed correctly
    assert CONF_AREAS in esphome_config
    areas = esphome_config[CONF_AREAS]
    assert len(areas) == 1
    assert areas[0]["id"].id == "bedroom_area"
    assert areas[0]["name"] == "Bedroom"

    # Verify devices were parsed correctly
    assert CONF_DEVICES in esphome_config
    devices = esphome_config[CONF_DEVICES]
    assert len(devices) == 1
    assert devices[0]["id"].id == "test_device"
    assert devices[0]["name"] == "Test Device"
    assert devices[0]["area_id"].id == "bedroom_area"


def test_multiple_areas_and_devices(yaml_file: Callable[[str], str]) -> None:
    """Test multiple areas and devices configuration."""
    result = load_config_from_fixture(
        yaml_file, "multiple_areas_devices.yaml", FIXTURES_DIR
    )
    assert result is not None

    esphome_config = result["esphome"]

    # Verify main area
    assert CONF_AREA in esphome_config
    main_area = esphome_config[CONF_AREA]
    assert main_area["id"].id == "main_area"
    assert main_area["name"] == "Main Area"

    # Verify additional areas
    assert CONF_AREAS in esphome_config
    areas = esphome_config[CONF_AREAS]
    assert len(areas) == 2
    area_ids = {area["id"].id for area in areas}
    assert area_ids == {"area1", "area2"}

    # Verify devices
    assert CONF_DEVICES in esphome_config
    devices = esphome_config[CONF_DEVICES]
    assert len(devices) == 3

    # Check device-area associations
    device_area_map = {dev["id"].id: dev["area_id"].id for dev in devices}
    assert device_area_map == {
        "device1": "main_area",
        "device2": "area1",
        "device3": "area2",
    }


def test_legacy_string_area(
    yaml_file: Callable[[str], str], caplog: pytest.LogCaptureFixture
) -> None:
    """Test legacy string area configuration with deprecation warning."""
    result = load_config_from_fixture(
        yaml_file, "legacy_string_area.yaml", FIXTURES_DIR
    )
    assert result is not None

    esphome_config = result["esphome"]

    # Verify the string was converted to structured format
    assert CONF_AREA in esphome_config
    area = esphome_config[CONF_AREA]
    assert isinstance(area, dict)
    assert area["name"] == "Living Room"
    assert isinstance(area["id"], core.ID)
    assert area["id"].is_declaration
    assert not area["id"].is_manual


def test_area_id_collision(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that duplicate area IDs are detected."""
    result = load_config_from_fixture(yaml_file, "area_id_collision.yaml", FIXTURES_DIR)
    assert result is None

    # Check for the specific error message in stdout
    captured = capsys.readouterr()
    # Exact duplicates are now caught by IDPassValidationStep
    assert "ID duplicate_id redefined! Check esphome->area->id." in captured.out


def test_device_without_area(yaml_file: Callable[[str], str]) -> None:
    """Test that devices without area_id work correctly."""
    result = load_config_from_fixture(
        yaml_file, "device_without_area.yaml", FIXTURES_DIR
    )
    assert result is not None

    esphome_config = result["esphome"]

    # Verify device was parsed
    assert CONF_DEVICES in esphome_config
    devices = esphome_config[CONF_DEVICES]
    assert len(devices) == 1

    device = devices[0]
    assert device["id"].id == "test_device"
    assert device["name"] == "Test Device"

    # Verify no area_id is present
    assert "area_id" not in device


def test_device_with_invalid_area_id(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that device with non-existent area_id fails validation."""
    result = load_config_from_fixture(
        yaml_file, "device_invalid_area.yaml", FIXTURES_DIR
    )
    assert result is None

    # Check for the specific error message in stdout
    captured = capsys.readouterr()
    assert (
        "Couldn't find ID 'nonexistent_area'. Please check you have defined an ID with that name in your configuration."
        in captured.out
    )


def test_device_id_hash_collision(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that device IDs with hash collisions are detected."""
    result = load_config_from_fixture(
        yaml_file, "device_id_collision.yaml", FIXTURES_DIR
    )
    assert result is None

    # Check for the specific error message about hash collision
    captured = capsys.readouterr()
    # The error message shows the ID that collides and includes the hash value
    assert (
        "Device ID 'd6ka' with hash 3082558663 collides with existing device ID 'test_2258'"
        in captured.out
    )


def test_area_id_hash_collision(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that area IDs with hash collisions are detected."""
    result = load_config_from_fixture(
        yaml_file, "area_id_hash_collision.yaml", FIXTURES_DIR
    )
    assert result is None

    # Check for the specific error message about hash collision
    captured = capsys.readouterr()
    # The error message shows the ID that collides and includes the hash value
    assert (
        "Area ID 'd6ka' with hash 3082558663 collides with existing area ID 'test_2258'"
        in captured.out
    )


def test_device_duplicate_id(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that duplicate device IDs are detected by IDPassValidationStep."""
    result = load_config_from_fixture(
        yaml_file, "device_duplicate_id.yaml", FIXTURES_DIR
    )
    assert result is None

    # Check for the specific error message from IDPassValidationStep
    captured = capsys.readouterr()
    assert "ID duplicate_device redefined!" in captured.out


def test_add_platform_defines_priority() -> None:
    """Test that _add_platform_defines runs after globals.

    This ensures the fix for issue #10431 where sensor counts were incorrect
    when lambdas were present. The function must run at a lower priority than
    globals (-100.0) to ensure all components (including those using globals
    in lambdas) have registered their entities before the count defines are
    generated.

    Regression test for https://github.com/esphome/esphome/issues/10431
    """
    # Import globals to check its priority
    from esphome.components.globals import to_code as globals_to_code

    # _add_platform_defines must run AFTER globals (lower priority number = runs later)
    assert config._add_platform_defines.priority < globals_to_code.priority, (
        f"_add_platform_defines priority ({config._add_platform_defines.priority}) must be lower than "
        f"globals priority ({globals_to_code.priority}) to fix issue #10431 (sensor count bug with lambdas)"
    )


def test_valid_include_with_angle_brackets() -> None:
    """Test valid_include accepts angle bracket includes."""
    assert valid_include("<ArduinoJson.h>") == "<ArduinoJson.h>"


def test_valid_include_with_valid_file(tmp_path: Path) -> None:
    """Test valid_include accepts valid include files."""
    CORE.config_path = str(tmp_path / "test.yaml")
    include_file = tmp_path / "include.h"
    include_file.touch()

    assert valid_include(str(include_file)) == str(include_file)


def test_valid_include_with_valid_directory(tmp_path: Path) -> None:
    """Test valid_include accepts valid directories."""
    CORE.config_path = str(tmp_path / "test.yaml")
    include_dir = tmp_path / "includes"
    include_dir.mkdir()

    assert valid_include(str(include_dir)) == str(include_dir)


def test_valid_include_invalid_extension(tmp_path: Path) -> None:
    """Test valid_include rejects files with invalid extensions."""
    CORE.config_path = str(tmp_path / "test.yaml")
    invalid_file = tmp_path / "file.txt"
    invalid_file.touch()

    with pytest.raises(cv.Invalid, match="Include has invalid file extension"):
        valid_include(str(invalid_file))


def test_valid_project_name_valid() -> None:
    """Test valid_project_name accepts valid project names."""
    assert valid_project_name("esphome.my_project") == "esphome.my_project"


def test_valid_project_name_no_namespace() -> None:
    """Test valid_project_name rejects names without namespace."""
    with pytest.raises(cv.Invalid, match="project name needs to have a namespace"):
        valid_project_name("my_project")


def test_valid_project_name_multiple_dots() -> None:
    """Test valid_project_name rejects names with multiple dots."""
    with pytest.raises(cv.Invalid, match="project name needs to have a namespace"):
        valid_project_name("esphome.my.project")


def test_validate_hostname_valid() -> None:
    """Test validate_hostname accepts valid hostnames."""
    config = {CONF_NAME: "my-device", CONF_NAME_ADD_MAC_SUFFIX: False}
    assert validate_hostname(config) == config


def test_validate_hostname_too_long() -> None:
    """Test validate_hostname rejects hostnames that are too long."""
    config = {
        CONF_NAME: "a" * 32,  # 32 chars, max is 31
        CONF_NAME_ADD_MAC_SUFFIX: False,
    }
    with pytest.raises(cv.Invalid, match="Hostnames can only be 31 characters long"):
        validate_hostname(config)


def test_validate_hostname_too_long_with_mac_suffix() -> None:
    """Test validate_hostname accounts for MAC suffix length."""
    config = {
        CONF_NAME: "a" * 25,  # 25 chars, max is 24 with MAC suffix
        CONF_NAME_ADD_MAC_SUFFIX: True,
    }
    with pytest.raises(cv.Invalid, match="Hostnames can only be 24 characters long"):
        validate_hostname(config)


def test_validate_hostname_with_underscore(caplog) -> None:
    """Test validate_hostname warns about underscores."""
    config = {CONF_NAME: "my_device", CONF_NAME_ADD_MAC_SUFFIX: False}
    assert validate_hostname(config) == config
    assert (
        "Using the '_' (underscore) character in the hostname is discouraged"
        in caplog.text
    )


def test_preload_core_config_basic(setup_core: Path) -> None:
    """Test preload_core_config sets basic CORE attributes."""
    config = {
        CONF_ESPHOME: {
            CONF_NAME: "test_device",
        },
        "esp32": {},
    }
    result = {}

    platform = preload_core_config(config, result)

    assert CORE.name == "test_device"
    assert platform == "esp32"
    assert KEY_CORE in CORE.data
    assert CONF_BUILD_PATH in config[CONF_ESPHOME]


def test_preload_core_config_with_build_path(setup_core: Path) -> None:
    """Test preload_core_config uses provided build path."""
    config = {
        CONF_ESPHOME: {
            CONF_NAME: "test_device",
            CONF_BUILD_PATH: "/custom/build/path",
        },
        "esp8266": {},
    }
    result = {}

    platform = preload_core_config(config, result)

    assert config[CONF_ESPHOME][CONF_BUILD_PATH] == "/custom/build/path"
    assert platform == "esp8266"


def test_preload_core_config_env_build_path(setup_core: Path) -> None:
    """Test preload_core_config uses ESPHOME_BUILD_PATH env var."""
    config = {
        CONF_ESPHOME: {
            CONF_NAME: "test_device",
        },
        "rp2040": {},
    }
    result = {}

    with patch.dict(os.environ, {"ESPHOME_BUILD_PATH": "/env/build"}):
        platform = preload_core_config(config, result)

    assert CONF_BUILD_PATH in config[CONF_ESPHOME]
    assert "test_device" in config[CONF_ESPHOME][CONF_BUILD_PATH]
    assert platform == "rp2040"


def test_preload_core_config_no_platform(setup_core: Path) -> None:
    """Test preload_core_config raises when no platform is specified."""
    config = {
        CONF_ESPHOME: {
            CONF_NAME: "test_device",
        },
    }
    result = {}

    # Mock _is_target_platform to avoid expensive component loading
    with patch("esphome.core.config._is_target_platform") as mock_is_platform:
        # Return True for known platforms
        mock_is_platform.side_effect = lambda name: name in [
            "esp32",
            "esp8266",
            "rp2040",
        ]

        with pytest.raises(cv.Invalid, match="Platform missing"):
            preload_core_config(config, result)


def test_preload_core_config_multiple_platforms(setup_core: Path) -> None:
    """Test preload_core_config raises when multiple platforms are specified."""
    config = {
        CONF_ESPHOME: {
            CONF_NAME: "test_device",
        },
        "esp32": {},
        "esp8266": {},
    }
    result = {}

    # Mock _is_target_platform to avoid expensive component loading
    with patch("esphome.core.config._is_target_platform") as mock_is_platform:
        # Return True for known platforms
        mock_is_platform.side_effect = lambda name: name in [
            "esp32",
            "esp8266",
            "rp2040",
        ]

        with pytest.raises(cv.Invalid, match="Found multiple target platform blocks"):
            preload_core_config(config, result)


def test_include_file_header(tmp_path: Path, mock_copy_file_if_changed: Mock) -> None:
    """Test include_file adds include statement for header files."""
    src_file = tmp_path / "source.h"
    src_file.write_text("// Header content")

    CORE.build_path = str(tmp_path / "build")

    with patch("esphome.core.config.cg") as mock_cg:
        # Mock RawStatement to capture the text
        mock_raw_statement = MagicMock()
        mock_raw_statement.text = ""

        def raw_statement_side_effect(text):
            mock_raw_statement.text = text
            return mock_raw_statement

        mock_cg.RawStatement.side_effect = raw_statement_side_effect

        config.include_file(str(src_file), "test.h")

        mock_copy_file_if_changed.assert_called_once()
        mock_cg.add_global.assert_called_once()
        # Check that include statement was added
        assert '#include "test.h"' in mock_raw_statement.text


def test_include_file_cpp(tmp_path: Path, mock_copy_file_if_changed: Mock) -> None:
    """Test include_file does not add include for cpp files."""
    src_file = tmp_path / "source.cpp"
    src_file.write_text("// CPP content")

    CORE.build_path = str(tmp_path / "build")

    with patch("esphome.core.config.cg") as mock_cg:
        config.include_file(str(src_file), "test.cpp")

        mock_copy_file_if_changed.assert_called_once()
        # Should not add include statement for .cpp files
        mock_cg.add_global.assert_not_called()


def test_get_usable_cpu_count() -> None:
    """Test get_usable_cpu_count returns CPU count."""
    count = config.get_usable_cpu_count()
    assert isinstance(count, int)
    assert count > 0


def test_get_usable_cpu_count_with_process_cpu_count() -> None:
    """Test get_usable_cpu_count uses process_cpu_count when available."""
    # Test with process_cpu_count (Python 3.13+)
    # Create a mock os module with process_cpu_count

    mock_os = types.SimpleNamespace(process_cpu_count=lambda: 8, cpu_count=lambda: 4)

    with patch("esphome.core.config.os", mock_os):
        # When process_cpu_count exists, it should be used
        count = config.get_usable_cpu_count()
        assert count == 8

    # Test fallback to cpu_count when process_cpu_count not available
    mock_os_no_process = types.SimpleNamespace(cpu_count=lambda: 4)

    with patch("esphome.core.config.os", mock_os_no_process):
        count = config.get_usable_cpu_count()
        assert count == 4


def test_list_target_platforms(tmp_path: Path) -> None:
    """Test _list_target_platforms returns available platforms."""
    # Create mock components directory structure
    components_dir = tmp_path / "components"
    components_dir.mkdir()

    # Create platform and non-platform directories with __init__.py
    platforms = ["esp32", "esp8266", "rp2040", "libretiny", "host"]
    non_platforms = ["sensor"]

    for component in platforms + non_platforms:
        component_dir = components_dir / component
        component_dir.mkdir()
        (component_dir / "__init__.py").touch()

    # Create a file (not a directory)
    (components_dir / "README.md").touch()

    # Create a directory without __init__.py
    (components_dir / "no_init").mkdir()

    # Mock Path(__file__).parents[1] to return our tmp_path
    with patch("esphome.core.config.Path") as mock_path:
        mock_file_path = MagicMock()
        mock_file_path.parents = [MagicMock(), tmp_path]
        mock_path.return_value = mock_file_path

        platforms = config._list_target_platforms()

    assert isinstance(platforms, list)
    # Should include platform components
    assert "esp32" in platforms
    assert "esp8266" in platforms
    assert "rp2040" in platforms
    assert "libretiny" in platforms
    assert "host" in platforms
    # Should not include non-platform components
    assert "sensor" not in platforms
    assert "README.md" not in platforms
    assert "no_init" not in platforms


def test_is_target_platform() -> None:
    """Test _is_target_platform identifies valid platforms."""
    assert config._is_target_platform("esp32") is True
    assert config._is_target_platform("esp8266") is True
    assert config._is_target_platform("rp2040") is True
    assert config._is_target_platform("invalid_platform") is False
    assert config._is_target_platform("api") is False  # Component but not platform
