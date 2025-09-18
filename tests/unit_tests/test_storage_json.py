"""Tests for storage_json.py path functions."""

from datetime import datetime
import json
from pathlib import Path
import sys
from unittest.mock import MagicMock, Mock, patch

import pytest

from esphome import storage_json
from esphome.const import CONF_DISABLED, CONF_MDNS
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


def test_storage_json_save_creates_directory(
    setup_core: Path, tmp_path: Path, mock_write_file_if_changed: Mock
) -> None:
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

    storage.save(str(storage_file))
    mock_write_file_if_changed.assert_called_once()
    call_args = mock_write_file_if_changed.call_args[0]
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


def test_storage_json_as_dict() -> None:
    """Test StorageJSON.as_dict returns correct dictionary."""
    storage = storage_json.StorageJSON(
        storage_version=1,
        name="test_device",
        friendly_name="Test Device",
        comment="Test comment",
        esphome_version="2024.1.0",
        src_version=1,
        address="192.168.1.100",
        web_port=80,
        target_platform="ESP32",
        build_path="/path/to/build",
        firmware_bin_path="/path/to/firmware.bin",
        loaded_integrations={"wifi", "api", "ota"},
        loaded_platforms={"sensor", "binary_sensor"},
        no_mdns=True,
        framework="arduino",
        core_platform="esp32",
    )

    result = storage.as_dict()

    assert result["storage_version"] == 1
    assert result["name"] == "test_device"
    assert result["friendly_name"] == "Test Device"
    assert result["comment"] == "Test comment"
    assert result["esphome_version"] == "2024.1.0"
    assert result["src_version"] == 1
    assert result["address"] == "192.168.1.100"
    assert result["web_port"] == 80
    assert result["esp_platform"] == "ESP32"
    assert result["build_path"] == "/path/to/build"
    assert result["firmware_bin_path"] == "/path/to/firmware.bin"
    assert "api" in result["loaded_integrations"]
    assert "wifi" in result["loaded_integrations"]
    assert "ota" in result["loaded_integrations"]
    assert result["loaded_integrations"] == sorted(
        ["wifi", "api", "ota"]
    )  # Should be sorted
    assert "sensor" in result["loaded_platforms"]
    assert result["loaded_platforms"] == sorted(
        ["sensor", "binary_sensor"]
    )  # Should be sorted
    assert result["no_mdns"] is True
    assert result["framework"] == "arduino"
    assert result["core_platform"] == "esp32"


def test_storage_json_to_json() -> None:
    """Test StorageJSON.to_json returns valid JSON string."""
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

    json_str = storage.to_json()

    # Should be valid JSON
    parsed = json.loads(json_str)
    assert parsed["name"] == "test"
    assert parsed["storage_version"] == 1

    # Should end with newline
    assert json_str.endswith("\n")


def test_storage_json_save(tmp_path: Path) -> None:
    """Test StorageJSON.save writes file correctly."""
    storage = storage_json.StorageJSON(
        storage_version=1,
        name="test",
        friendly_name="Test",
        comment=None,
        esphome_version="2024.1.0",
        src_version=None,
        address="test.local",
        web_port=None,
        target_platform="ESP32",
        build_path=None,
        firmware_bin_path=None,
        loaded_integrations=set(),
        loaded_platforms=set(),
        no_mdns=False,
    )

    save_path = tmp_path / "test.json"

    with patch("esphome.storage_json.write_file_if_changed") as mock_write:
        storage.save(str(save_path))
        mock_write.assert_called_once_with(str(save_path), storage.to_json())


def test_storage_json_from_esphome_core(setup_core: Path) -> None:
    """Test StorageJSON.from_esphome_core creates correct storage object."""
    # Mock CORE object
    mock_core = MagicMock()
    mock_core.name = "my_device"
    mock_core.friendly_name = "My Device"
    mock_core.comment = "A test device"
    mock_core.address = "192.168.1.50"
    mock_core.web_port = 8080
    mock_core.target_platform = "esp32"
    mock_core.is_esp32 = True
    mock_core.build_path = "/build/my_device"
    mock_core.firmware_bin = "/build/my_device/firmware.bin"
    mock_core.loaded_integrations = {"wifi", "api"}
    mock_core.loaded_platforms = {"sensor"}
    mock_core.config = {CONF_MDNS: {CONF_DISABLED: True}}
    mock_core.target_framework = "esp-idf"

    with patch("esphome.components.esp32.get_esp32_variant") as mock_variant:
        mock_variant.return_value = "ESP32-C3"

        result = storage_json.StorageJSON.from_esphome_core(mock_core, old=None)

    assert result.name == "my_device"
    assert result.friendly_name == "My Device"
    assert result.comment == "A test device"
    assert result.address == "192.168.1.50"
    assert result.web_port == 8080
    assert result.target_platform == "ESP32-C3"
    assert result.build_path == "/build/my_device"
    assert result.firmware_bin_path == "/build/my_device/firmware.bin"
    assert result.loaded_integrations == {"wifi", "api"}
    assert result.loaded_platforms == {"sensor"}
    assert result.no_mdns is True
    assert result.framework == "esp-idf"
    assert result.core_platform == "esp32"


def test_storage_json_from_esphome_core_mdns_enabled(setup_core: Path) -> None:
    """Test from_esphome_core with mDNS enabled."""
    mock_core = MagicMock()
    mock_core.name = "test"
    mock_core.friendly_name = "Test"
    mock_core.comment = None
    mock_core.address = "test.local"
    mock_core.web_port = None
    mock_core.target_platform = "esp8266"
    mock_core.is_esp32 = False
    mock_core.build_path = "/build"
    mock_core.firmware_bin = "/build/firmware.bin"
    mock_core.loaded_integrations = set()
    mock_core.loaded_platforms = set()
    mock_core.config = {}  # No MDNS config means enabled
    mock_core.target_framework = "arduino"

    result = storage_json.StorageJSON.from_esphome_core(mock_core, old=None)

    assert result.no_mdns is False


def test_storage_json_load_valid_file(tmp_path: Path) -> None:
    """Test StorageJSON.load with valid JSON file."""
    storage_data = {
        "storage_version": 1,
        "name": "loaded_device",
        "friendly_name": "Loaded Device",
        "comment": "Loaded from file",
        "esphome_version": "2024.1.0",
        "src_version": 2,
        "address": "10.0.0.1",
        "web_port": 8080,
        "esp_platform": "ESP32",
        "build_path": "/loaded/build",
        "firmware_bin_path": "/loaded/firmware.bin",
        "loaded_integrations": ["wifi", "api"],
        "loaded_platforms": ["sensor"],
        "no_mdns": True,
        "framework": "arduino",
        "core_platform": "esp32",
    }

    file_path = tmp_path / "storage.json"
    file_path.write_text(json.dumps(storage_data))

    result = storage_json.StorageJSON.load(str(file_path))

    assert result is not None
    assert result.name == "loaded_device"
    assert result.friendly_name == "Loaded Device"
    assert result.comment == "Loaded from file"
    assert result.esphome_version == "2024.1.0"
    assert result.src_version == 2
    assert result.address == "10.0.0.1"
    assert result.web_port == 8080
    assert result.target_platform == "ESP32"
    assert result.build_path == "/loaded/build"
    assert result.firmware_bin_path == "/loaded/firmware.bin"
    assert result.loaded_integrations == {"wifi", "api"}
    assert result.loaded_platforms == {"sensor"}
    assert result.no_mdns is True
    assert result.framework == "arduino"
    assert result.core_platform == "esp32"


def test_storage_json_load_invalid_file(tmp_path: Path) -> None:
    """Test StorageJSON.load with invalid JSON file."""
    file_path = tmp_path / "invalid.json"
    file_path.write_text("not valid json{")

    result = storage_json.StorageJSON.load(str(file_path))

    assert result is None


def test_storage_json_load_nonexistent_file() -> None:
    """Test StorageJSON.load with non-existent file."""
    result = storage_json.StorageJSON.load("/nonexistent/file.json")

    assert result is None


def test_storage_json_equality() -> None:
    """Test StorageJSON equality comparison."""
    storage1 = storage_json.StorageJSON(
        storage_version=1,
        name="test",
        friendly_name="Test",
        comment=None,
        esphome_version="2024.1.0",
        src_version=1,
        address="test.local",
        web_port=80,
        target_platform="ESP32",
        build_path="/build",
        firmware_bin_path="/firmware.bin",
        loaded_integrations={"wifi"},
        loaded_platforms=set(),
        no_mdns=False,
    )

    storage2 = storage_json.StorageJSON(
        storage_version=1,
        name="test",
        friendly_name="Test",
        comment=None,
        esphome_version="2024.1.0",
        src_version=1,
        address="test.local",
        web_port=80,
        target_platform="ESP32",
        build_path="/build",
        firmware_bin_path="/firmware.bin",
        loaded_integrations={"wifi"},
        loaded_platforms=set(),
        no_mdns=False,
    )

    storage3 = storage_json.StorageJSON(
        storage_version=1,
        name="different",  # Different name
        friendly_name="Test",
        comment=None,
        esphome_version="2024.1.0",
        src_version=1,
        address="test.local",
        web_port=80,
        target_platform="ESP32",
        build_path="/build",
        firmware_bin_path="/firmware.bin",
        loaded_integrations={"wifi"},
        loaded_platforms=set(),
        no_mdns=False,
    )

    assert storage1 == storage2
    assert storage1 != storage3
    assert storage1 != "not a storage object"


def test_esphome_storage_json_as_dict() -> None:
    """Test EsphomeStorageJSON.as_dict returns correct dictionary."""
    storage = storage_json.EsphomeStorageJSON(
        storage_version=1,
        cookie_secret="secret123",
        last_update_check="2024-01-15T10:30:00",
        remote_version="2024.1.1",
    )

    result = storage.as_dict()

    assert result["storage_version"] == 1
    assert result["cookie_secret"] == "secret123"
    assert result["last_update_check"] == "2024-01-15T10:30:00"
    assert result["remote_version"] == "2024.1.1"


def test_esphome_storage_json_last_update_check_property() -> None:
    """Test EsphomeStorageJSON.last_update_check property."""
    storage = storage_json.EsphomeStorageJSON(
        storage_version=1,
        cookie_secret="secret",
        last_update_check="2024-01-15T10:30:00",
        remote_version=None,
    )

    # Test getter
    result = storage.last_update_check
    assert isinstance(result, datetime)
    assert result.year == 2024
    assert result.month == 1
    assert result.day == 15
    assert result.hour == 10
    assert result.minute == 30

    # Test setter
    new_date = datetime(2024, 2, 20, 15, 45, 30)
    storage.last_update_check = new_date
    assert storage.last_update_check_str == "2024-02-20T15:45:30"


def test_esphome_storage_json_last_update_check_invalid() -> None:
    """Test EsphomeStorageJSON.last_update_check with invalid date."""
    storage = storage_json.EsphomeStorageJSON(
        storage_version=1,
        cookie_secret="secret",
        last_update_check="invalid date",
        remote_version=None,
    )

    result = storage.last_update_check
    assert result is None


def test_esphome_storage_json_to_json() -> None:
    """Test EsphomeStorageJSON.to_json returns valid JSON string."""
    storage = storage_json.EsphomeStorageJSON(
        storage_version=1,
        cookie_secret="mysecret",
        last_update_check="2024-01-15T10:30:00",
        remote_version="2024.1.1",
    )

    json_str = storage.to_json()

    # Should be valid JSON
    parsed = json.loads(json_str)
    assert parsed["cookie_secret"] == "mysecret"
    assert parsed["storage_version"] == 1

    # Should end with newline
    assert json_str.endswith("\n")


def test_esphome_storage_json_save(tmp_path: Path) -> None:
    """Test EsphomeStorageJSON.save writes file correctly."""
    storage = storage_json.EsphomeStorageJSON(
        storage_version=1,
        cookie_secret="secret",
        last_update_check=None,
        remote_version=None,
    )

    save_path = tmp_path / "esphome.json"

    with patch("esphome.storage_json.write_file_if_changed") as mock_write:
        storage.save(str(save_path))
        mock_write.assert_called_once_with(str(save_path), storage.to_json())


def test_esphome_storage_json_load_valid_file(tmp_path: Path) -> None:
    """Test EsphomeStorageJSON.load with valid JSON file."""
    storage_data = {
        "storage_version": 1,
        "cookie_secret": "loaded_secret",
        "last_update_check": "2024-01-20T14:30:00",
        "remote_version": "2024.1.2",
    }

    file_path = tmp_path / "esphome.json"
    file_path.write_text(json.dumps(storage_data))

    result = storage_json.EsphomeStorageJSON.load(str(file_path))

    assert result is not None
    assert result.storage_version == 1
    assert result.cookie_secret == "loaded_secret"
    assert result.last_update_check_str == "2024-01-20T14:30:00"
    assert result.remote_version == "2024.1.2"


def test_esphome_storage_json_load_invalid_file(tmp_path: Path) -> None:
    """Test EsphomeStorageJSON.load with invalid JSON file."""
    file_path = tmp_path / "invalid.json"
    file_path.write_text("not valid json{")

    result = storage_json.EsphomeStorageJSON.load(str(file_path))

    assert result is None


def test_esphome_storage_json_load_nonexistent_file() -> None:
    """Test EsphomeStorageJSON.load with non-existent file."""
    result = storage_json.EsphomeStorageJSON.load("/nonexistent/file.json")

    assert result is None


def test_esphome_storage_json_get_default() -> None:
    """Test EsphomeStorageJSON.get_default creates default storage."""
    with patch("esphome.storage_json.os.urandom") as mock_urandom:
        # Mock urandom to return predictable bytes
        mock_urandom.return_value = b"test" * 16  # 64 bytes

        result = storage_json.EsphomeStorageJSON.get_default()

    assert result.storage_version == 1
    assert len(result.cookie_secret) == 128  # 64 bytes hex = 128 chars
    assert result.last_update_check is None
    assert result.remote_version is None


def test_esphome_storage_json_equality() -> None:
    """Test EsphomeStorageJSON equality comparison."""
    storage1 = storage_json.EsphomeStorageJSON(
        storage_version=1,
        cookie_secret="secret",
        last_update_check="2024-01-15T10:30:00",
        remote_version="2024.1.1",
    )

    storage2 = storage_json.EsphomeStorageJSON(
        storage_version=1,
        cookie_secret="secret",
        last_update_check="2024-01-15T10:30:00",
        remote_version="2024.1.1",
    )

    storage3 = storage_json.EsphomeStorageJSON(
        storage_version=1,
        cookie_secret="different",  # Different secret
        last_update_check="2024-01-15T10:30:00",
        remote_version="2024.1.1",
    )

    assert storage1 == storage2
    assert storage1 != storage3
    assert storage1 != "not a storage object"


def test_storage_json_load_legacy_esphomeyaml_version(tmp_path: Path) -> None:
    """Test loading storage with legacy esphomeyaml_version field."""
    storage_data = {
        "storage_version": 1,
        "name": "legacy_device",
        "friendly_name": "Legacy Device",
        "esphomeyaml_version": "1.14.0",  # Legacy field name
        "address": "legacy.local",
        "esp_platform": "ESP8266",
    }

    file_path = tmp_path / "legacy.json"
    file_path.write_text(json.dumps(storage_data))

    result = storage_json.StorageJSON.load(str(file_path))

    assert result is not None
    assert result.esphome_version == "1.14.0"  # Should map to esphome_version
