from __future__ import annotations

import asyncio
from collections.abc import Generator
from contextlib import asynccontextmanager
import gzip
import json
import os
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, Mock, patch

import pytest
import pytest_asyncio
from tornado.httpclient import AsyncHTTPClient, HTTPClientError, HTTPResponse
from tornado.httpserver import HTTPServer
from tornado.ioloop import IOLoop
from tornado.testing import bind_unused_port
from tornado.websocket import WebSocketClientConnection, websocket_connect

from esphome.dashboard import web_server
from esphome.dashboard.const import DashboardEvent
from esphome.dashboard.core import DASHBOARD
from esphome.dashboard.entries import (
    DashboardEntry,
    EntryStateSource,
    bool_to_entry_state,
)
from esphome.dashboard.models import build_importable_device_dict
from esphome.dashboard.web_server import DashboardSubscriber
from esphome.zeroconf import DiscoveredImport

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
        "esphome.dashboard.web_server.trash_storage_path", return_value=trash_dir
    ) as mock:
        yield mock


@pytest.fixture
def mock_archive_storage_path(tmp_path: Path) -> Generator[MagicMock]:
    """Fixture to mock archive_storage_path."""
    archive_dir = tmp_path / "archive"
    with patch(
        "esphome.dashboard.web_server.archive_storage_path",
        return_value=archive_dir,
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


@asynccontextmanager
async def websocket_connection(dashboard: DashboardTestHelper):
    """Async context manager for WebSocket connections."""
    url = f"ws://127.0.0.1:{dashboard.port}/events"
    ws = await websocket_connect(url)
    try:
        yield ws
    finally:
        if ws:
            ws.close()


@pytest_asyncio.fixture
async def websocket_client(dashboard: DashboardTestHelper) -> WebSocketClientConnection:
    """Create a WebSocket connection for testing."""
    url = f"ws://127.0.0.1:{dashboard.port}/events"
    ws = await websocket_connect(url)

    # Read and discard initial state message
    await ws.read_message()

    yield ws

    if ws:
        ws.close()


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
    mock_storage.firmware_bin_path = firmware_file
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
    mock_storage.firmware_bin_path = firmware_file
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
    mock_storage.firmware_bin_path = firmware_file
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
    mock_storage.firmware_bin_path = firmware_file
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
    mock_dashboard_settings.rel_path.return_value = test_file
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
    archive_dir = mock_archive_storage_path.return_value
    archive_dir.mkdir(parents=True, exist_ok=True)
    archived_file = archive_dir / "archived.yaml"
    archived_file.write_text("test content")

    # Set up the destination path where the file should be moved
    config_dir = tmp_path / "config"
    config_dir.mkdir(parents=True, exist_ok=True)
    destination_file = config_dir / "archived.yaml"
    mock_dashboard_settings.rel_path.return_value = destination_file

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
    mock_dashboard_settings.rel_path.return_value = secrets_file

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
    trash_dir = mock_trash_storage_path.return_value
    archive_dir = mock_archive_storage_path.return_value

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


@pytest.mark.asyncio
async def test_edit_request_handler_get(dashboard: DashboardTestHelper) -> None:
    """Test EditRequestHandler.get method."""
    # Test getting a valid yaml file
    response = await dashboard.fetch("/edit?configuration=pico.yaml")
    assert response.code == 200
    assert response.headers["content-type"] == "application/yaml"
    content = response.body.decode()
    assert "esphome:" in content  # Verify it's a valid ESPHome config

    # Test getting a non-existent file
    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch("/edit?configuration=nonexistent.yaml")
    assert exc_info.value.code == 404

    # Test getting a non-yaml file
    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch("/edit?configuration=test.txt")
    assert exc_info.value.code == 404

    # Test path traversal attempt
    with pytest.raises(HTTPClientError) as exc_info:
        await dashboard.fetch("/edit?configuration=../../../etc/passwd")
    assert exc_info.value.code == 404


@pytest.mark.asyncio
async def test_archive_request_handler_post(
    dashboard: DashboardTestHelper,
    mock_archive_storage_path: MagicMock,
    mock_ext_storage_path: MagicMock,
    tmp_path: Path,
) -> None:
    """Test ArchiveRequestHandler.post method without storage_json."""

    # Set up temp directories
    config_dir = Path(get_fixture_path("conf"))
    archive_dir = tmp_path / "archive"

    # Create a test configuration file
    test_config = config_dir / "test_archive.yaml"
    test_config.write_text("esphome:\n  name: test_archive\n")

    # Archive the configuration
    response = await dashboard.fetch(
        "/archive",
        method="POST",
        body="configuration=test_archive.yaml",
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    assert response.code == 200

    # Verify file was moved to archive
    assert not test_config.exists()
    assert (archive_dir / "test_archive.yaml").exists()
    assert (
        archive_dir / "test_archive.yaml"
    ).read_text() == "esphome:\n  name: test_archive\n"


@pytest.mark.asyncio
async def test_archive_handler_with_build_folder(
    dashboard: DashboardTestHelper,
    mock_archive_storage_path: MagicMock,
    mock_ext_storage_path: MagicMock,
    mock_dashboard_settings: MagicMock,
    mock_storage_json: MagicMock,
    tmp_path: Path,
) -> None:
    """Test ArchiveRequestHandler.post with storage_json and build folder."""
    config_dir = tmp_path / "config"
    config_dir.mkdir()
    archive_dir = tmp_path / "archive"
    archive_dir.mkdir()
    build_dir = tmp_path / "build"
    build_dir.mkdir()

    configuration = "test_device.yaml"
    test_config = config_dir / configuration
    test_config.write_text("esphome:\n  name: test_device\n")

    build_folder = build_dir / "test_device"
    build_folder.mkdir()
    (build_folder / "firmware.bin").write_text("binary content")
    (build_folder / ".pioenvs").mkdir()

    mock_dashboard_settings.config_dir = str(config_dir)
    mock_dashboard_settings.rel_path.return_value = test_config
    mock_archive_storage_path.return_value = archive_dir

    mock_storage = MagicMock()
    mock_storage.name = "test_device"
    mock_storage.build_path = build_folder
    mock_storage_json.load.return_value = mock_storage

    response = await dashboard.fetch(
        "/archive",
        method="POST",
        body=f"configuration={configuration}",
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    assert response.code == 200

    assert not test_config.exists()
    assert (archive_dir / configuration).exists()

    assert not build_folder.exists()
    assert not (archive_dir / "test_device").exists()


@pytest.mark.asyncio
async def test_archive_handler_no_build_folder(
    dashboard: DashboardTestHelper,
    mock_archive_storage_path: MagicMock,
    mock_ext_storage_path: MagicMock,
    mock_dashboard_settings: MagicMock,
    mock_storage_json: MagicMock,
    tmp_path: Path,
) -> None:
    """Test ArchiveRequestHandler.post with storage_json but no build folder."""
    config_dir = tmp_path / "config"
    config_dir.mkdir()
    archive_dir = tmp_path / "archive"
    archive_dir.mkdir()

    configuration = "test_device.yaml"
    test_config = config_dir / configuration
    test_config.write_text("esphome:\n  name: test_device\n")

    mock_dashboard_settings.config_dir = str(config_dir)
    mock_dashboard_settings.rel_path.return_value = test_config
    mock_archive_storage_path.return_value = archive_dir

    mock_storage = MagicMock()
    mock_storage.name = "test_device"
    mock_storage.build_path = None
    mock_storage_json.load.return_value = mock_storage

    response = await dashboard.fetch(
        "/archive",
        method="POST",
        body=f"configuration={configuration}",
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    assert response.code == 200

    assert not test_config.exists()
    assert (archive_dir / configuration).exists()
    assert not (archive_dir / "test_device").exists()


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


def test_build_cache_arguments_no_entry(mock_dashboard: Mock) -> None:
    """Test with no entry returns empty list."""
    result = web_server.build_cache_arguments(None, mock_dashboard, 0.0)
    assert result == []


def test_build_cache_arguments_no_address_no_name(mock_dashboard: Mock) -> None:
    """Test with entry but no address or name."""
    entry = Mock(spec=web_server.DashboardEntry)
    entry.address = None
    entry.name = None
    result = web_server.build_cache_arguments(entry, mock_dashboard, 0.0)
    assert result == []


def test_build_cache_arguments_mdns_address_cached(mock_dashboard: Mock) -> None:
    """Test with .local address that has cached mDNS results."""
    entry = Mock(spec=web_server.DashboardEntry)
    entry.address = "device.local"
    entry.name = None
    mock_dashboard.mdns_status = Mock()
    mock_dashboard.mdns_status.get_cached_addresses.return_value = [
        "192.168.1.10",
        "fe80::1",
    ]

    result = web_server.build_cache_arguments(entry, mock_dashboard, 0.0)

    assert result == [
        "--mdns-address-cache",
        "device.local=192.168.1.10,fe80::1",
    ]
    mock_dashboard.mdns_status.get_cached_addresses.assert_called_once_with(
        "device.local"
    )


def test_build_cache_arguments_dns_address_cached(mock_dashboard: Mock) -> None:
    """Test with non-.local address that has cached DNS results."""
    entry = Mock(spec=web_server.DashboardEntry)
    entry.address = "example.com"
    entry.name = None
    mock_dashboard.dns_cache = Mock()
    mock_dashboard.dns_cache.get_cached_addresses.return_value = [
        "93.184.216.34",
        "2606:2800:220:1:248:1893:25c8:1946",
    ]

    now = 100.0
    result = web_server.build_cache_arguments(entry, mock_dashboard, now)

    # IPv6 addresses are sorted before IPv4
    assert result == [
        "--dns-address-cache",
        "example.com=2606:2800:220:1:248:1893:25c8:1946,93.184.216.34",
    ]
    mock_dashboard.dns_cache.get_cached_addresses.assert_called_once_with(
        "example.com", now
    )


def test_build_cache_arguments_name_without_address(mock_dashboard: Mock) -> None:
    """Test with name but no address - should check mDNS with .local suffix."""
    entry = Mock(spec=web_server.DashboardEntry)
    entry.name = "my-device"
    entry.address = None
    mock_dashboard.mdns_status = Mock()
    mock_dashboard.mdns_status.get_cached_addresses.return_value = ["192.168.1.20"]

    result = web_server.build_cache_arguments(entry, mock_dashboard, 0.0)

    assert result == [
        "--mdns-address-cache",
        "my-device.local=192.168.1.20",
    ]
    mock_dashboard.mdns_status.get_cached_addresses.assert_called_once_with(
        "my-device.local"
    )


@pytest.mark.asyncio
async def test_websocket_connection_initial_state(
    dashboard: DashboardTestHelper,
) -> None:
    """Test WebSocket connection and initial state."""
    async with websocket_connection(dashboard) as ws:
        # Should receive initial state with configured and importable devices
        msg = await ws.read_message()
        assert msg is not None
        data = json.loads(msg)
        assert data["event"] == "initial_state"
        assert "devices" in data["data"]
        assert "configured" in data["data"]["devices"]
        assert "importable" in data["data"]["devices"]

        # Check configured devices
        configured = data["data"]["devices"]["configured"]
        assert len(configured) > 0
        assert configured[0]["name"] == "pico"  # From test fixtures


@pytest.mark.asyncio
async def test_websocket_ping_pong(
    dashboard: DashboardTestHelper, websocket_client: WebSocketClientConnection
) -> None:
    """Test WebSocket ping/pong mechanism."""
    # Send ping
    await websocket_client.write_message(json.dumps({"event": "ping"}))

    # Should receive pong
    msg = await websocket_client.read_message()
    assert msg is not None
    data = json.loads(msg)
    assert data["event"] == "pong"


@pytest.mark.asyncio
async def test_websocket_invalid_json(
    dashboard: DashboardTestHelper, websocket_client: WebSocketClientConnection
) -> None:
    """Test WebSocket handling of invalid JSON."""
    # Send invalid JSON
    await websocket_client.write_message("not valid json {]")

    # Send a valid ping to verify connection is still alive
    await websocket_client.write_message(json.dumps({"event": "ping"}))

    # Should receive pong, confirming the connection wasn't closed by invalid JSON
    msg = await websocket_client.read_message()
    assert msg is not None
    data = json.loads(msg)
    assert data["event"] == "pong"


@pytest.mark.asyncio
async def test_websocket_authentication_required(
    dashboard: DashboardTestHelper,
) -> None:
    """Test WebSocket authentication when auth is required."""
    with patch(
        "esphome.dashboard.web_server.is_authenticated"
    ) as mock_is_authenticated:
        mock_is_authenticated.return_value = False

        # Try to connect - should be rejected with 401
        url = f"ws://127.0.0.1:{dashboard.port}/events"
        with pytest.raises(HTTPClientError) as exc_info:
            await websocket_connect(url)
        # Should get HTTP 401 Unauthorized
        assert exc_info.value.code == 401


@pytest.mark.asyncio
async def test_websocket_authentication_not_required(
    dashboard: DashboardTestHelper,
) -> None:
    """Test WebSocket connection when no auth is required."""
    with patch(
        "esphome.dashboard.web_server.is_authenticated"
    ) as mock_is_authenticated:
        mock_is_authenticated.return_value = True

        # Should be able to connect successfully
        async with websocket_connection(dashboard) as ws:
            msg = await ws.read_message()
            assert msg is not None
            data = json.loads(msg)
            assert data["event"] == "initial_state"


@pytest.mark.asyncio
async def test_websocket_entry_state_changed(
    dashboard: DashboardTestHelper, websocket_client: WebSocketClientConnection
) -> None:
    """Test WebSocket entry state changed event."""
    # Simulate entry state change
    entry = DASHBOARD.entries.async_all()[0]
    state = bool_to_entry_state(True, EntryStateSource.MDNS)
    DASHBOARD.bus.async_fire(
        DashboardEvent.ENTRY_STATE_CHANGED, {"entry": entry, "state": state}
    )

    # Should receive state change event
    msg = await websocket_client.read_message()
    assert msg is not None
    data = json.loads(msg)
    assert data["event"] == "entry_state_changed"
    assert data["data"]["filename"] == entry.filename
    assert data["data"]["name"] == entry.name
    assert data["data"]["state"] is True


@pytest.mark.asyncio
async def test_websocket_entry_added(
    dashboard: DashboardTestHelper, websocket_client: WebSocketClientConnection
) -> None:
    """Test WebSocket entry added event."""
    # Create a mock entry
    mock_entry = Mock(spec=DashboardEntry)
    mock_entry.filename = "test.yaml"
    mock_entry.name = "test_device"
    mock_entry.to_dict.return_value = {
        "name": "test_device",
        "filename": "test.yaml",
        "configuration": "test.yaml",
    }

    # Simulate entry added
    DASHBOARD.bus.async_fire(DashboardEvent.ENTRY_ADDED, {"entry": mock_entry})

    # Should receive entry added event
    msg = await websocket_client.read_message()
    assert msg is not None
    data = json.loads(msg)
    assert data["event"] == "entry_added"
    assert data["data"]["device"]["name"] == "test_device"
    assert data["data"]["device"]["filename"] == "test.yaml"


@pytest.mark.asyncio
async def test_websocket_entry_removed(
    dashboard: DashboardTestHelper, websocket_client: WebSocketClientConnection
) -> None:
    """Test WebSocket entry removed event."""
    # Create a mock entry
    mock_entry = Mock(spec=DashboardEntry)
    mock_entry.filename = "removed.yaml"
    mock_entry.name = "removed_device"
    mock_entry.to_dict.return_value = {
        "name": "removed_device",
        "filename": "removed.yaml",
        "configuration": "removed.yaml",
    }

    # Simulate entry removed
    DASHBOARD.bus.async_fire(DashboardEvent.ENTRY_REMOVED, {"entry": mock_entry})

    # Should receive entry removed event
    msg = await websocket_client.read_message()
    assert msg is not None
    data = json.loads(msg)
    assert data["event"] == "entry_removed"
    assert data["data"]["device"]["name"] == "removed_device"
    assert data["data"]["device"]["filename"] == "removed.yaml"


@pytest.mark.asyncio
async def test_websocket_importable_device_added(
    dashboard: DashboardTestHelper, websocket_client: WebSocketClientConnection
) -> None:
    """Test WebSocket importable device added event with real DiscoveredImport."""
    # Create a real DiscoveredImport object
    discovered = DiscoveredImport(
        device_name="new_import_device",
        friendly_name="New Import Device",
        package_import_url="https://example.com/package",
        project_name="test_project",
        project_version="1.0.0",
        network="wifi",
    )

    # Directly fire the event as the mDNS system would
    device_dict = build_importable_device_dict(DASHBOARD, discovered)
    DASHBOARD.bus.async_fire(
        DashboardEvent.IMPORTABLE_DEVICE_ADDED, {"device": device_dict}
    )

    # Should receive importable device added event
    msg = await websocket_client.read_message()
    assert msg is not None
    data = json.loads(msg)
    assert data["event"] == "importable_device_added"
    assert data["data"]["device"]["name"] == "new_import_device"
    assert data["data"]["device"]["friendly_name"] == "New Import Device"
    assert data["data"]["device"]["project_name"] == "test_project"
    assert data["data"]["device"]["network"] == "wifi"
    assert data["data"]["device"]["ignored"] is False


@pytest.mark.asyncio
async def test_websocket_importable_device_added_ignored(
    dashboard: DashboardTestHelper, websocket_client: WebSocketClientConnection
) -> None:
    """Test WebSocket importable device added event for ignored device."""
    # Add device to ignored list
    DASHBOARD.ignored_devices.add("ignored_device")

    # Create a real DiscoveredImport object
    discovered = DiscoveredImport(
        device_name="ignored_device",
        friendly_name="Ignored Device",
        package_import_url="https://example.com/package",
        project_name="test_project",
        project_version="1.0.0",
        network="ethernet",
    )

    # Directly fire the event as the mDNS system would
    device_dict = build_importable_device_dict(DASHBOARD, discovered)
    DASHBOARD.bus.async_fire(
        DashboardEvent.IMPORTABLE_DEVICE_ADDED, {"device": device_dict}
    )

    # Should receive importable device added event with ignored=True
    msg = await websocket_client.read_message()
    assert msg is not None
    data = json.loads(msg)
    assert data["event"] == "importable_device_added"
    assert data["data"]["device"]["name"] == "ignored_device"
    assert data["data"]["device"]["friendly_name"] == "Ignored Device"
    assert data["data"]["device"]["network"] == "ethernet"
    assert data["data"]["device"]["ignored"] is True


@pytest.mark.asyncio
async def test_websocket_importable_device_removed(
    dashboard: DashboardTestHelper, websocket_client: WebSocketClientConnection
) -> None:
    """Test WebSocket importable device removed event."""
    # Simulate importable device removed
    DASHBOARD.bus.async_fire(
        DashboardEvent.IMPORTABLE_DEVICE_REMOVED,
        {"name": "removed_import_device"},
    )

    # Should receive importable device removed event
    msg = await websocket_client.read_message()
    assert msg is not None
    data = json.loads(msg)
    assert data["event"] == "importable_device_removed"
    assert data["data"]["name"] == "removed_import_device"


@pytest.mark.asyncio
async def test_websocket_importable_device_already_configured(
    dashboard: DashboardTestHelper, websocket_client: WebSocketClientConnection
) -> None:
    """Test that importable device event is not sent if device is already configured."""
    # Get an existing configured device name
    existing_entry = DASHBOARD.entries.async_all()[0]

    # Simulate importable device added with same name as configured device
    DASHBOARD.bus.async_fire(
        DashboardEvent.IMPORTABLE_DEVICE_ADDED,
        {
            "device": {
                "name": existing_entry.name,
                "friendly_name": "Should Not Be Sent",
                "package_import_url": "https://example.com/package",
                "project_name": "test_project",
                "project_version": "1.0.0",
                "network": "wifi",
            }
        },
    )

    # Send a ping to ensure connection is still alive
    await websocket_client.write_message(json.dumps({"event": "ping"}))

    # Should only receive pong, not the importable device event
    msg = await websocket_client.read_message()
    assert msg is not None
    data = json.loads(msg)
    assert data["event"] == "pong"


@pytest.mark.asyncio
async def test_websocket_multiple_connections(dashboard: DashboardTestHelper) -> None:
    """Test multiple WebSocket connections."""
    async with (
        websocket_connection(dashboard) as ws1,
        websocket_connection(dashboard) as ws2,
    ):
        # Both should receive initial state
        msg1 = await ws1.read_message()
        assert msg1 is not None
        data1 = json.loads(msg1)
        assert data1["event"] == "initial_state"

        msg2 = await ws2.read_message()
        assert msg2 is not None
        data2 = json.loads(msg2)
        assert data2["event"] == "initial_state"

        # Fire an event - both should receive it
        entry = DASHBOARD.entries.async_all()[0]
        state = bool_to_entry_state(False, EntryStateSource.MDNS)
        DASHBOARD.bus.async_fire(
            DashboardEvent.ENTRY_STATE_CHANGED, {"entry": entry, "state": state}
        )

        msg1 = await ws1.read_message()
        assert msg1 is not None
        data1 = json.loads(msg1)
        assert data1["event"] == "entry_state_changed"

        msg2 = await ws2.read_message()
        assert msg2 is not None
        data2 = json.loads(msg2)
        assert data2["event"] == "entry_state_changed"


@pytest.mark.asyncio
async def test_dashboard_subscriber_lifecycle(dashboard: DashboardTestHelper) -> None:
    """Test DashboardSubscriber lifecycle."""
    subscriber = DashboardSubscriber()

    # Initially no subscribers
    assert len(subscriber._subscribers) == 0
    assert subscriber._event_loop_task is None

    # Add a subscriber
    mock_websocket = Mock()
    unsubscribe = subscriber.subscribe(mock_websocket)

    # Should have started the event loop task
    assert len(subscriber._subscribers) == 1
    assert subscriber._event_loop_task is not None

    # Unsubscribe
    unsubscribe()

    # Should have stopped the task
    assert len(subscriber._subscribers) == 0


@pytest.mark.asyncio
async def test_dashboard_subscriber_entries_update_interval(
    dashboard: DashboardTestHelper,
) -> None:
    """Test DashboardSubscriber entries update interval."""
    # Patch the constants to make the test run faster
    with (
        patch("esphome.dashboard.web_server.DASHBOARD_POLL_INTERVAL", 0.01),
        patch("esphome.dashboard.web_server.DASHBOARD_ENTRIES_UPDATE_ITERATIONS", 2),
        patch("esphome.dashboard.web_server.settings") as mock_settings,
        patch("esphome.dashboard.web_server.DASHBOARD") as mock_dashboard,
    ):
        mock_settings.status_use_mqtt = False

        # Mock dashboard dependencies
        mock_dashboard.ping_request = Mock()
        mock_dashboard.ping_request.set = Mock()
        mock_dashboard.entries = Mock()
        mock_dashboard.entries.async_request_update_entries = Mock()

        subscriber = DashboardSubscriber()
        mock_websocket = Mock()

        # Subscribe to start the event loop
        unsubscribe = subscriber.subscribe(mock_websocket)

        # Wait for a few iterations to ensure entries update is called
        await asyncio.sleep(0.05)  # Should be enough for 2+ iterations

        # Unsubscribe to stop the task
        unsubscribe()

        # Verify entries update was called
        assert mock_dashboard.entries.async_request_update_entries.call_count >= 1
        # Verify ping request was set multiple times
        assert mock_dashboard.ping_request.set.call_count >= 2


@pytest.mark.asyncio
async def test_websocket_refresh_command(
    dashboard: DashboardTestHelper, websocket_client: WebSocketClientConnection
) -> None:
    """Test WebSocket refresh command triggers dashboard update."""
    with patch("esphome.dashboard.web_server.DASHBOARD_SUBSCRIBER") as mock_subscriber:
        mock_subscriber.request_refresh = Mock()

        # Send refresh command
        await websocket_client.write_message(json.dumps({"event": "refresh"}))

        # Give it a moment to process
        await asyncio.sleep(0.01)

        # Verify request_refresh was called
        mock_subscriber.request_refresh.assert_called_once()


@pytest.mark.asyncio
async def test_dashboard_subscriber_refresh_event(
    dashboard: DashboardTestHelper,
) -> None:
    """Test DashboardSubscriber refresh event triggers immediate update."""
    # Patch the constants to make the test run faster
    with (
        patch(
            "esphome.dashboard.web_server.DASHBOARD_POLL_INTERVAL", 1.0
        ),  # Long timeout
        patch(
            "esphome.dashboard.web_server.DASHBOARD_ENTRIES_UPDATE_ITERATIONS", 100
        ),  # Won't reach naturally
        patch("esphome.dashboard.web_server.settings") as mock_settings,
        patch("esphome.dashboard.web_server.DASHBOARD") as mock_dashboard,
    ):
        mock_settings.status_use_mqtt = False

        # Mock dashboard dependencies
        mock_dashboard.ping_request = Mock()
        mock_dashboard.ping_request.set = Mock()
        mock_dashboard.entries = Mock()
        mock_dashboard.entries.async_request_update_entries = AsyncMock()

        subscriber = DashboardSubscriber()
        mock_websocket = Mock()

        # Subscribe to start the event loop
        unsubscribe = subscriber.subscribe(mock_websocket)

        # Wait a bit to ensure loop is running
        await asyncio.sleep(0.01)

        # Verify entries update hasn't been called yet (iterations not reached)
        assert mock_dashboard.entries.async_request_update_entries.call_count == 0

        # Request refresh
        subscriber.request_refresh()

        # Wait for the refresh to be processed
        await asyncio.sleep(0.01)

        # Now entries update should have been called
        assert mock_dashboard.entries.async_request_update_entries.call_count == 1

        # Unsubscribe to stop the task
        unsubscribe()

        # Give it a moment to clean up
        await asyncio.sleep(0.01)
