"""Tests for storage_json.py path functions."""

from pathlib import Path
import sys
from unittest.mock import patch

import pytest

from esphome import storage_json
from esphome.core import CORE


def test_storage_path(setup_core: Path) -> None:
    """Test storage_path returns correct path for current config."""
    CORE.config_path = str(setup_core / "my_device.yaml")

    result = storage_json.storage_path()

    data_dir = Path(CORE.data_dir)
    expected = str(data_dir / "storage" / "my_device.yaml.json")
    assert result == expected


def test_ext_storage_path(setup_core: Path) -> None:
    """Test ext_storage_path returns correct path for given filename."""
    result = storage_json.ext_storage_path("other_device.yaml")

    data_dir = Path(CORE.data_dir)
    expected = str(data_dir / "storage" / "other_device.yaml.json")
    assert result == expected


def test_ext_storage_path_handles_various_extensions(setup_core: Path) -> None:
    """Test ext_storage_path works with different file extensions."""
    result_yml = storage_json.ext_storage_path("device.yml")
    assert result_yml.endswith("device.yml.json")

    result_no_ext = storage_json.ext_storage_path("device")
    assert result_no_ext.endswith("device.json")

    result_path = storage_json.ext_storage_path("my/device.yaml")
    assert result_path.endswith("device.yaml.json")


def test_esphome_storage_path(setup_core: Path) -> None:
    """Test esphome_storage_path returns correct path."""
    result = storage_json.esphome_storage_path()

    data_dir = Path(CORE.data_dir)
    expected = str(data_dir / "esphome.json")
    assert result == expected


def test_ignored_devices_storage_path(setup_core: Path) -> None:
    """Test ignored_devices_storage_path returns correct path."""
    result = storage_json.ignored_devices_storage_path()

    data_dir = Path(CORE.data_dir)
    expected = str(data_dir / "ignored-devices.json")
    assert result == expected


def test_trash_storage_path(setup_core: Path) -> None:
    """Test trash_storage_path returns correct path."""
    CORE.config_path = str(setup_core / "configs" / "device.yaml")

    result = storage_json.trash_storage_path()

    expected = str(setup_core / "configs" / "trash")
    assert result == expected


def test_archive_storage_path(setup_core: Path) -> None:
    """Test archive_storage_path returns correct path."""
    CORE.config_path = str(setup_core / "configs" / "device.yaml")

    result = storage_json.archive_storage_path()

    expected = str(setup_core / "configs" / "archive")
    assert result == expected


def test_storage_path_with_subdirectory(setup_core: Path) -> None:
    """Test storage paths work correctly when config is in subdirectory."""
    subdir = setup_core / "configs" / "basement"
    subdir.mkdir(parents=True, exist_ok=True)
    CORE.config_path = str(subdir / "sensor.yaml")

    result = storage_json.storage_path()

    data_dir = Path(CORE.data_dir)
    expected = str(data_dir / "storage" / "sensor.yaml.json")
    assert result == expected


def test_storage_json_firmware_bin_path_property(setup_core: Path) -> None:
    """Test StorageJSON firmware_bin_path property."""
    storage = storage_json.StorageJSON(
        storage_version=1,
        name="test_device",
        friendly_name="Test Device",
        comment=None,
        esphome_version="2024.1.0",
        src_version=None,
        address="192.168.1.100",
        web_port=80,
        target_platform="ESP32",
        build_path="build/test_device",
        firmware_bin_path="/path/to/firmware.bin",
        loaded_integrations={"wifi", "api"},
        loaded_platforms=set(),
        no_mdns=False,
    )

    assert storage.firmware_bin_path == "/path/to/firmware.bin"


def test_storage_json_save_creates_directory(setup_core: Path, tmp_path: Path) -> None:
    """Test StorageJSON.save creates storage directory if it doesn't exist."""
    storage_dir = tmp_path / "new_data" / "storage"
    storage_file = storage_dir / "test.json"

    assert not storage_dir.exists()

    storage = storage_json.StorageJSON(
        storage_version=1,
        name="test",
        friendly_name="Test",
        comment=None,
        esphome_version="2024.1.0",
        src_version=None,
        address="test.local",
        web_port=None,
        target_platform="ESP8266",
        build_path=None,
        firmware_bin_path=None,
        loaded_integrations=set(),
        loaded_platforms=set(),
        no_mdns=False,
    )

    with patch("esphome.storage_json.write_file_if_changed") as mock_write:
        storage.save(str(storage_file))
        mock_write.assert_called_once()
        call_args = mock_write.call_args[0]
        assert call_args[0] == str(storage_file)


def test_storage_json_from_wizard(setup_core: Path) -> None:
    """Test StorageJSON.from_wizard creates correct storage object."""
    storage = storage_json.StorageJSON.from_wizard(
        name="my_device",
        friendly_name="My Device",
        address="my_device.local",
        platform="ESP32",
    )

    assert storage.name == "my_device"
    assert storage.friendly_name == "My Device"
    assert storage.address == "my_device.local"
    assert storage.target_platform == "ESP32"
    assert storage.build_path is None
    assert storage.firmware_bin_path is None


@pytest.mark.skipif(sys.platform == "win32", reason="HA addons don't run on Windows")
@patch("esphome.core.is_ha_addon")
def test_storage_paths_with_ha_addon(mock_is_ha_addon: bool, tmp_path: Path) -> None:
    """Test storage paths when running as Home Assistant addon."""
    mock_is_ha_addon.return_value = True

    CORE.config_path = str(tmp_path / "test.yaml")

    result = storage_json.storage_path()
    # When is_ha_addon is True, CORE.data_dir returns "/data"
    # This is the standard mount point for HA addon containers
    expected = str(Path("/data") / "storage" / "test.yaml.json")
    assert result == expected

    result = storage_json.esphome_storage_path()
    expected = str(Path("/data") / "esphome.json")
    assert result == expected
