from __future__ import annotations

import asyncio
from collections.abc import Generator
import gzip
import json
import os
from pathlib import Path
from unittest.mock import MagicMock, Mock, patch

import pytest
import pytest_asyncio
from tornado.httpclient import AsyncHTTPClient, HTTPClientError, HTTPResponse
from tornado.httpserver import HTTPServer
from tornado.ioloop import IOLoop
from tornado.testing import bind_unused_port

from esphome.dashboard import web_server
from esphome.dashboard.core import DASHBOARD

from .common import get_fixture_path


class DashboardTestHelper:
    def __init__(self, io_loop: IOLoop, client: AsyncHTTPClient, port: int) -> None:
        self.io_loop = io_loop
        self.client = client
        self.port = port

    async def fetch(self, path: str, **kwargs) -> HTTPResponse:
        """Get a response for the given path."""
        if path.lower().startswith(("http://", "https://")):
            url = path
        else:
            url = f"http://127.0.0.1:{self.port}{path}"
        future = self.client.fetch(url, raise_error=True, **kwargs)
        return await future


@pytest.fixture
def mock_async_run_system_command() -> Generator[MagicMock]:
    """Fixture to mock async_run_system_command."""
    with patch("esphome.dashboard.web_server.async_run_system_command") as mock:
        yield mock


@pytest.fixture
def mock_trash_storage_path(tmp_path: Path) -> Generator[MagicMock]:
    """Fixture to mock trash_storage_path."""
    trash_dir = tmp_path / "trash"
    with patch(
        "esphome.dashboard.web_server.trash_storage_path", return_value=str(trash_dir)
    ) as mock:
        yield mock


@pytest.fixture
def mock_archive_storage_path(tmp_path: Path) -> Generator[MagicMock]:
    """Fixture to mock archive_storage_path."""
    archive_dir = tmp_path / "archive"
    with patch(
        "esphome.dashboard.web_server.archive_storage_path",
        return_value=str(archive_dir),
    ) as mock:
        yield mock


@pytest.fixture
def mock_dashboard_settings() -> Generator[MagicMock]:
    """Fixture to mock dashboard settings."""
    with patch("esphome.dashboard.web_server.settings") as mock_settings:
        # Set default auth settings to avoid authentication issues
        mock_settings.using_auth = False
        mock_settings.on_ha_addon = False
        yield mock_settings


@pytest.fixture
def mock_ext_storage_path(tmp_path: Path) -> Generator[MagicMock]:
    """Fixture to mock ext_storage_path."""
    with patch("esphome.dashboard.web_server.ext_storage_path") as mock:
        mock.return_value = str(tmp_path / "storage.json")
        yield mock


@pytest.fixture
def mock_storage_json() -> Generator[MagicMock]:
    """Fixture to mock StorageJSON."""
    with patch("esphome.dashboard.web_server.StorageJSON") as mock:
        yield mock


@pytest.fixture
def mock_idedata() -> Generator[MagicMock]:
    """Fixture to mock platformio_api.IDEData."""
    with patch("esphome.dashboard.web_server.platformio_api.IDEData") as mock:
        yield mock


@pytest_asyncio.fixture()
async def dashboard() -> DashboardTestHelper:
    sock, port = bind_unused_port()
    args = Mock(
        ha_addon=True,
        configuration=get_fixture_path("conf"),
        port=port,
    )
    DASHBOARD.settings.parse_args(args)
    app = web_server.make_app()
    http_server = HTTPServer(app)
    http_server.add_sockets([sock])
    await DASHBOARD.async_setup()
    os.environ["DISABLE_HA_AUTHENTICATION"] = "1"
    assert DASHBOARD.settings.using_password is False
    assert DASHBOARD.settings.on_ha_addon is True
    assert DASHBOARD.settings.using_auth is False
    task = asyncio.create_task(DASHBOARD.async_run())
    # Wait for initial device loading to complete
    await DASHBOARD.entries.async_request_update_entries()
    client = AsyncHTTPClient()
    io_loop = IOLoop(make_current=False)
    yield DashboardTestHelper(io_loop, client, port)
    task.cancel()
    sock.close()
    client.close()
    io_loop.close()


@pytest.mark.asyncio
async def test_main_page(dashboard: DashboardTestHelper) -> None:
    response = await dashboard.fetch("/")
    assert response.code == 200


@pytest.mark.asyncio
async def test_devices_page(dashboard: DashboardTestHelper) -> None:
    response = await dashboard.fetch("/devices")
    assert response.code == 200
    assert response.headers["content-type"] == "application/json"
    json_data = json.loads(response.body.decode())
    configured_devices = json_data["configured"]
    assert len(configured_devices) != 0
    first_device = configured_devices[0]
    assert first_device["name"] == "pico"
    assert first_device["configuration"] == "pico.yaml"


@pytest.mark.asyncio
async def test_wizard_handler_invalid_input(dashboard: DashboardTestHelper) -> None:
    """Test the WizardRequestHandler.post method with invalid inputs."""
    # Test with missing name (should fail with 422)
    body_no_name = json.dumps(
        {
            "name": "",  # Empty name
            "platform": "ESP32",
            "board": "esp32dev",
        }
    )
    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch(
            "/wizard",
            method="POST",
            body=body_no_name,
            headers={"Content-Type": "application/json"},
        )
    assert exc_info.value.code == 422

    # Test with invalid wizard type (should fail with 422)
    body_invalid_type = json.dumps(
        {
            "name": "test_device",
            "type": "invalid_type",
            "platform": "ESP32",
            "board": "esp32dev",
        }
    )
    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch(
            "/wizard",
            method="POST",
            body=body_invalid_type,
            headers={"Content-Type": "application/json"},
        )
    assert exc_info.value.code == 422


@pytest.mark.asyncio
async def test_wizard_handler_conflict(dashboard: DashboardTestHelper) -> None:
    """Test the WizardRequestHandler.post when config already exists."""
    # Try to create a wizard for existing pico.yaml (should conflict)
    body = json.dumps(
        {
            "name": "pico",  # This already exists in fixtures
            "platform": "ESP32",
            "board": "esp32dev",
        }
    )
    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch(
            "/wizard",
            method="POST",
            body=body,
            headers={"Content-Type": "application/json"},
        )
    assert exc_info.value.code == 409


@pytest.mark.asyncio
async def test_download_binary_handler_not_found(
    dashboard: DashboardTestHelper,
) -> None:
    """Test the DownloadBinaryRequestHandler.get with non-existent config."""
    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch(
            "/download.bin?configuration=nonexistent.yaml",
            method="GET",
        )
    assert exc_info.value.code == 404


@pytest.mark.asyncio
@pytest.mark.usefixtures("mock_ext_storage_path")
async def test_download_binary_handler_no_file_param(
    dashboard: DashboardTestHelper,
    tmp_path: Path,
    mock_storage_json: MagicMock,
) -> None:
    """Test the DownloadBinaryRequestHandler.get without file parameter."""
    # Mock storage to exist, but still should fail without file param
    mock_storage = Mock()
    mock_storage.name = "test_device"
    mock_storage.firmware_bin_path = str(tmp_path / "firmware.bin")
    mock_storage_json.load.return_value = mock_storage

    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch(
            "/download.bin?configuration=pico.yaml",
            method="GET",
        )
    assert exc_info.value.code == 400


@pytest.mark.asyncio
@pytest.mark.usefixtures("mock_ext_storage_path")
async def test_download_binary_handler_with_file(
    dashboard: DashboardTestHelper,
    tmp_path: Path,
    mock_storage_json: MagicMock,
) -> None:
    """Test the DownloadBinaryRequestHandler.get with existing binary file."""
    # Create a fake binary file
    build_dir = tmp_path / ".esphome" / "build" / "test"
    build_dir.mkdir(parents=True)
    firmware_file = build_dir / "firmware.bin"
    firmware_file.write_bytes(b"fake firmware content")

    # Mock storage JSON
    mock_storage = Mock()
    mock_storage.name = "test_device"
    mock_storage.firmware_bin_path = str(firmware_file)
    mock_storage_json.load.return_value = mock_storage

    response = await dashboard.fetch(
        "/download.bin?configuration=test.yaml&file=firmware.bin",
        method="GET",
    )
    assert response.code == 200
    assert response.body == b"fake firmware content"
    assert response.headers["Content-Type"] == "application/octet-stream"
    assert "attachment" in response.headers["Content-Disposition"]
    assert "test_device-firmware.bin" in response.headers["Content-Disposition"]


@pytest.mark.asyncio
@pytest.mark.usefixtures("mock_ext_storage_path")
async def test_download_binary_handler_compressed(
    dashboard: DashboardTestHelper,
    tmp_path: Path,
    mock_storage_json: MagicMock,
) -> None:
    """Test the DownloadBinaryRequestHandler.get with compression."""
    # Create a fake binary file
    build_dir = tmp_path / ".esphome" / "build" / "test"
    build_dir.mkdir(parents=True)
    firmware_file = build_dir / "firmware.bin"
    original_content = b"fake firmware content for compression test"
    firmware_file.write_bytes(original_content)

    # Mock storage JSON
    mock_storage = Mock()
    mock_storage.name = "test_device"
    mock_storage.firmware_bin_path = str(firmware_file)
    mock_storage_json.load.return_value = mock_storage

    response = await dashboard.fetch(
        "/download.bin?configuration=test.yaml&file=firmware.bin&compressed=1",
        method="GET",
    )
    assert response.code == 200
    # Decompress and verify content
    decompressed = gzip.decompress(response.body)
    assert decompressed == original_content
    assert response.headers["Content-Type"] == "application/octet-stream"
    assert "firmware.bin.gz" in response.headers["Content-Disposition"]


@pytest.mark.asyncio
@pytest.mark.usefixtures("mock_ext_storage_path")
async def test_download_binary_handler_custom_download_name(
    dashboard: DashboardTestHelper,
    tmp_path: Path,
    mock_storage_json: MagicMock,
) -> None:
    """Test the DownloadBinaryRequestHandler.get with custom download name."""
    # Create a fake binary file
    build_dir = tmp_path / ".esphome" / "build" / "test"
    build_dir.mkdir(parents=True)
    firmware_file = build_dir / "firmware.bin"
    firmware_file.write_bytes(b"content")

    # Mock storage JSON
    mock_storage = Mock()
    mock_storage.name = "test_device"
    mock_storage.firmware_bin_path = str(firmware_file)
    mock_storage_json.load.return_value = mock_storage

    response = await dashboard.fetch(
        "/download.bin?configuration=test.yaml&file=firmware.bin&download=custom_name.bin",
        method="GET",
    )
    assert response.code == 200
    assert "custom_name.bin" in response.headers["Content-Disposition"]


@pytest.mark.asyncio
@pytest.mark.usefixtures("mock_ext_storage_path")
async def test_download_binary_handler_idedata_fallback(
    dashboard: DashboardTestHelper,
    tmp_path: Path,
    mock_async_run_system_command: MagicMock,
    mock_storage_json: MagicMock,
    mock_idedata: MagicMock,
) -> None:
    """Test the DownloadBinaryRequestHandler.get falling back to idedata for extra images."""
    # Create build directory but no bootloader file initially
    build_dir = tmp_path / ".esphome" / "build" / "test"
    build_dir.mkdir(parents=True)
    firmware_file = build_dir / "firmware.bin"
    firmware_file.write_bytes(b"firmware")

    # Create bootloader file that idedata will find
    bootloader_file = tmp_path / "bootloader.bin"
    bootloader_file.write_bytes(b"bootloader content")

    # Mock storage JSON
    mock_storage = Mock()
    mock_storage.name = "test_device"
    mock_storage.firmware_bin_path = str(firmware_file)
    mock_storage_json.load.return_value = mock_storage

    # Mock idedata response
    mock_image = Mock()
    mock_image.path = str(bootloader_file)
    mock_idedata_instance = Mock()
    mock_idedata_instance.extra_flash_images = [mock_image]
    mock_idedata.return_value = mock_idedata_instance

    # Mock async_run_system_command to return idedata JSON
    mock_async_run_system_command.return_value = (0, '{"extra_flash_images": []}', "")

    response = await dashboard.fetch(
        "/download.bin?configuration=test.yaml&file=bootloader.bin",
        method="GET",
    )
    assert response.code == 200
    assert response.body == b"bootloader content"


@pytest.mark.asyncio
async def test_edit_request_handler_post_invalid_file(
    dashboard: DashboardTestHelper,
) -> None:
    """Test the EditRequestHandler.post with non-yaml file."""
    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch(
            "/edit?configuration=test.txt",
            method="POST",
            body=b"content",
        )
    assert exc_info.value.code == 404


@pytest.mark.asyncio
async def test_edit_request_handler_post_existing(
    dashboard: DashboardTestHelper,
    tmp_path: Path,
    mock_dashboard_settings: MagicMock,
) -> None:
    """Test the EditRequestHandler.post with existing yaml file."""
    # Create a temporary yaml file to edit (don't modify fixtures)
    test_file = tmp_path / "test_edit.yaml"
    test_file.write_text("esphome:\n  name: original\n")

    # Configure the mock settings
    mock_dashboard_settings.rel_path.return_value = str(test_file)
    mock_dashboard_settings.absolute_config_dir = test_file.parent

    new_content = "esphome:\n  name: modified\n"
    response = await dashboard.fetch(
        "/edit?configuration=test_edit.yaml",
        method="POST",
        body=new_content.encode(),
    )
    assert response.code == 200

    # Verify the file was actually modified
    assert test_file.read_text() == new_content


@pytest.mark.asyncio
async def test_unarchive_request_handler(
    dashboard: DashboardTestHelper,
    mock_archive_storage_path: MagicMock,
    mock_dashboard_settings: MagicMock,
    tmp_path: Path,
) -> None:
    """Test the UnArchiveRequestHandler.post method."""
    # Set up an archived file
    archive_dir = Path(mock_archive_storage_path.return_value)
    archive_dir.mkdir(parents=True, exist_ok=True)
    archived_file = archive_dir / "archived.yaml"
    archived_file.write_text("test content")

    # Set up the destination path where the file should be moved
    config_dir = tmp_path / "config"
    config_dir.mkdir(parents=True, exist_ok=True)
    destination_file = config_dir / "archived.yaml"
    mock_dashboard_settings.rel_path.return_value = str(destination_file)

    response = await dashboard.fetch(
        "/unarchive?configuration=archived.yaml",
        method="POST",
        body=b"",
    )
    assert response.code == 200

    # Verify the file was actually moved from archive to config
    assert not archived_file.exists()  # File should be gone from archive
    assert destination_file.exists()  # File should now be in config
    assert destination_file.read_text() == "test content"  # Content preserved


@pytest.mark.asyncio
async def test_secret_keys_handler_no_file(dashboard: DashboardTestHelper) -> None:
    """Test the SecretKeysRequestHandler.get when no secrets file exists."""
    # By default, there's no secrets file in the test fixtures
    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch("/secret_keys", method="GET")
    assert exc_info.value.code == 404


@pytest.mark.asyncio
async def test_secret_keys_handler_with_file(
    dashboard: DashboardTestHelper,
    tmp_path: Path,
    mock_dashboard_settings: MagicMock,
) -> None:
    """Test the SecretKeysRequestHandler.get when secrets file exists."""
    # Create a secrets file in temp directory
    secrets_file = tmp_path / "secrets.yaml"
    secrets_file.write_text(
        "wifi_ssid: TestNetwork\nwifi_password: TestPass123\napi_key: test_key\n"
    )

    # Configure mock to return our temp secrets file
    # Since the file actually exists, os.path.isfile will return True naturally
    mock_dashboard_settings.rel_path.return_value = str(secrets_file)

    response = await dashboard.fetch("/secret_keys", method="GET")
    assert response.code == 200
    data = json.loads(response.body.decode())
    assert "wifi_ssid" in data
    assert "wifi_password" in data
    assert "api_key" in data


@pytest.mark.asyncio
async def test_json_config_handler(
    dashboard: DashboardTestHelper,
    mock_async_run_system_command: MagicMock,
) -> None:
    """Test the JsonConfigRequestHandler.get method."""
    # This will actually run the esphome config command on pico.yaml
    mock_output = json.dumps(
        {
            "esphome": {"name": "pico"},
            "esp32": {"board": "esp32dev"},
        }
    )
    mock_async_run_system_command.return_value = (0, mock_output, "")

    response = await dashboard.fetch(
        "/json-config?configuration=pico.yaml", method="GET"
    )
    assert response.code == 200
    data = json.loads(response.body.decode())
    assert data["esphome"]["name"] == "pico"


@pytest.mark.asyncio
async def test_json_config_handler_invalid_config(
    dashboard: DashboardTestHelper,
    mock_async_run_system_command: MagicMock,
) -> None:
    """Test the JsonConfigRequestHandler.get with invalid config."""
    # Simulate esphome config command failure
    mock_async_run_system_command.return_value = (1, "", "Error: Invalid configuration")

    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch("/json-config?configuration=pico.yaml", method="GET")
    assert exc_info.value.code == 422


@pytest.mark.asyncio
async def test_json_config_handler_not_found(dashboard: DashboardTestHelper) -> None:
    """Test the JsonConfigRequestHandler.get with non-existent file."""
    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch(
            "/json-config?configuration=nonexistent.yaml", method="GET"
        )
    assert exc_info.value.code == 404


def test_start_web_server_with_address_port(
    tmp_path: Path,
    mock_trash_storage_path: MagicMock,
    mock_archive_storage_path: MagicMock,
) -> None:
    """Test the start_web_server function with address and port."""
    app = Mock()
    trash_dir = Path(mock_trash_storage_path.return_value)
    archive_dir = Path(mock_archive_storage_path.return_value)

    # Create trash dir to test migration
    trash_dir.mkdir()
    (trash_dir / "old.yaml").write_text("old")

    web_server.start_web_server(app, None, "127.0.0.1", 6052, str(tmp_path / "config"))

    # The function calls app.listen directly for non-socket mode
    app.listen.assert_called_once_with(6052, "127.0.0.1")

    # Verify trash was moved to archive
    assert not trash_dir.exists()
    assert archive_dir.exists()
    assert (archive_dir / "old.yaml").exists()


@pytest.mark.skipif(os.name == "nt", reason="Unix sockets are not supported on Windows")
@pytest.mark.usefixtures("mock_trash_storage_path", "mock_archive_storage_path")
def test_start_web_server_with_unix_socket(tmp_path: Path) -> None:
    """Test the start_web_server function with unix socket."""
    app = Mock()
    socket_path = tmp_path / "test.sock"

    # Don't create trash_dir - it doesn't exist, so no migration needed
    with (
        patch("tornado.httpserver.HTTPServer") as mock_server_class,
        patch("tornado.netutil.bind_unix_socket") as mock_bind,
    ):
        server = Mock()
        mock_server_class.return_value = server
        mock_bind.return_value = Mock()

        web_server.start_web_server(
            app, str(socket_path), None, None, str(tmp_path / "config")
        )

        mock_server_class.assert_called_once_with(app)
        mock_bind.assert_called_once_with(str(socket_path), mode=0o666)
        server.add_socket.assert_called_once()
