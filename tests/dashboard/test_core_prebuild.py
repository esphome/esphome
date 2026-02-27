"""Tests for the dashboard pre-build firmware feature."""

from __future__ import annotations

from collections.abc import Generator
import json
from pathlib import Path
from unittest.mock import AsyncMock, Mock, patch

import pytest

from esphome.dashboard.const import DashboardEvent
from esphome.dashboard.core import (
    ESPHomeDashboard,
    _cleanup_old_firmware,
    _restore_storage_version,
)


def _make_entry(
    filename: str = "device.yaml",
    name: str = "device",
    update_available: bool = True,
    update_old: str = "2026.1.0",
) -> Mock:
    """Create a mock DashboardEntry for testing."""
    entry = Mock()
    entry.filename = filename
    entry.name = name
    entry.update_available = update_available
    entry.update_old = update_old
    return entry


@pytest.fixture
def mock_ext_storage_path(tmp_path: Path) -> Generator[Mock]:
    """Fixture to mock ext_storage_path in core module."""
    storage_file = tmp_path / "device.yaml.json"
    with patch(
        "esphome.dashboard.core.ext_storage_path", return_value=storage_file
    ) as mock:
        mock.storage_file = storage_file
        yield mock


@pytest.fixture
def mock_async_run_system_command() -> Generator[AsyncMock]:
    """Fixture to mock async_run_system_command in core module."""
    with patch(
        "esphome.dashboard.core.async_run_system_command",
        new_callable=AsyncMock,
    ) as mock:
        yield mock


@pytest.fixture
def mock_restore_storage_version() -> Generator[Mock]:
    """Fixture to mock _restore_storage_version."""
    with patch("esphome.dashboard.core._restore_storage_version") as mock:
        yield mock


@pytest.fixture
def mock_storage_json_load() -> Generator[Mock]:
    """Fixture to mock StorageJSON.load in core module (returns None by default)."""
    with patch(
        "esphome.dashboard.core.StorageJSON.load", return_value=None
    ) as mock:
        yield mock


@pytest.fixture
def prebuild_dashboard(
    mock_storage_json_load: Mock, mock_ext_storage_path: Mock
) -> ESPHomeDashboard:
    """Create an ESPHomeDashboard with mocked bus and loop for pre-build testing."""
    dash = ESPHomeDashboard()
    dash.bus = Mock()
    dash.entries = Mock()
    dash.settings = Mock()
    dash.settings.rel_path = Mock(side_effect=lambda f: f)
    dash.loop = Mock()
    dash.loop.run_in_executor = AsyncMock(
        side_effect=lambda executor, func, *args: func(*args)
    )
    return dash


# -- _restore_storage_version tests --


def test_restore_storage_version_when_different(
    mock_ext_storage_path: Mock, tmp_path: Path
) -> None:
    """Test version is restored when storage version differs from old_version."""
    storage_file = mock_ext_storage_path.storage_file
    storage_file.write_text(
        json.dumps({"esphome_version": "2026.2.0", "name": "device"})
    )

    entry = Mock()
    entry.filename = "device.yaml"

    _restore_storage_version(entry, "2026.1.0")

    data = json.loads(storage_file.read_text())
    assert data["esphome_version"] == "2026.1.0"
    assert data["name"] == "device"
    entry.load_from_disk.assert_called_once()


def test_restore_storage_version_no_op_when_matches(
    mock_ext_storage_path: Mock, tmp_path: Path
) -> None:
    """Test no-op when version already matches old_version."""
    storage_file = mock_ext_storage_path.storage_file
    storage_file.write_text(
        json.dumps({"esphome_version": "2026.1.0", "name": "device"})
    )

    entry = Mock()
    entry.filename = "device.yaml"

    _restore_storage_version(entry, "2026.1.0")

    entry.load_from_disk.assert_not_called()


def test_restore_storage_version_handles_missing_file() -> None:
    """Test missing storage file does not raise."""
    entry = Mock()
    entry.filename = "missing.yaml"

    with patch(
        "esphome.dashboard.core.ext_storage_path",
        return_value=Path("/nonexistent/path.json"),
    ):
        _restore_storage_version(entry, "2026.1.0")


def test_restore_storage_version_handles_corrupt_json(
    mock_ext_storage_path: Mock,
) -> None:
    """Test corrupt JSON in storage file does not raise."""
    storage_file = mock_ext_storage_path.storage_file
    storage_file.write_text("not valid json{{{")

    entry = Mock()
    entry.filename = "device.yaml"

    _restore_storage_version(entry, "2026.1.0")


# -- _async_prebuild_devices tests --


@pytest.mark.asyncio
async def test_prebuild_skips_when_no_devices_need_update(
    prebuild_dashboard: ESPHomeDashboard,
) -> None:
    """Test pre-build is skipped when no devices have update_available."""
    entry = _make_entry(update_available=False)
    prebuild_dashboard.entries.async_all.return_value = [entry]

    await prebuild_dashboard._async_prebuild_devices()

    prebuild_dashboard.bus.async_fire.assert_not_called()


@pytest.mark.asyncio
async def test_prebuild_successful_build(
    prebuild_dashboard: ESPHomeDashboard,
    mock_async_run_system_command: AsyncMock,
) -> None:
    """Test successful compile fires started, device_done, and finished events."""
    entry = _make_entry()
    prebuild_dashboard.entries.async_all.return_value = [entry]
    mock_async_run_system_command.return_value = (0, b"", b"")

    await prebuild_dashboard._async_prebuild_devices()

    calls = prebuild_dashboard.bus.async_fire.call_args_list
    assert len(calls) == 3

    assert calls[0].args[0] == DashboardEvent.PRE_BUILD_STATUS
    assert calls[0].args[1]["status"] == "started"
    assert calls[0].args[1]["total"] == 1

    assert calls[1].args[0] == DashboardEvent.PRE_BUILD_STATUS
    assert calls[1].args[1]["status"] == "device_done"
    assert calls[1].args[1]["filename"] == "device.yaml"

    assert calls[2].args[0] == DashboardEvent.PRE_BUILD_STATUS
    assert calls[2].args[1]["status"] == "finished"
    assert calls[2].args[1]["succeeded"] == 1
    assert calls[2].args[1]["failed"] == 0

    entry.load_from_disk.assert_called_once()


@pytest.mark.asyncio
async def test_prebuild_failed_build(
    prebuild_dashboard: ESPHomeDashboard,
    mock_async_run_system_command: AsyncMock,
    mock_ext_storage_path: Mock,
) -> None:
    """Test failed compile fires device_failed and restores storage version."""
    entry = _make_entry()
    prebuild_dashboard.entries.async_all.return_value = [entry]
    mock_async_run_system_command.return_value = (1, b"", b"Error: compilation failed")

    storage_file = mock_ext_storage_path.storage_file
    storage_file.write_text(
        json.dumps({"esphome_version": "2026.2.0", "name": "device"})
    )

    await prebuild_dashboard._async_prebuild_devices()

    calls = prebuild_dashboard.bus.async_fire.call_args_list
    assert len(calls) == 3

    assert calls[1].args[1]["status"] == "device_failed"
    assert calls[1].args[1]["filename"] == "device.yaml"
    assert "compilation failed" in calls[1].args[1]["error"]

    assert calls[2].args[1]["succeeded"] == 0
    assert calls[2].args[1]["failed"] == 1

    data = json.loads(storage_file.read_text())
    assert data["esphome_version"] == "2026.1.0"


@pytest.mark.asyncio
async def test_prebuild_exception_during_build(
    prebuild_dashboard: ESPHomeDashboard,
    mock_async_run_system_command: AsyncMock,
    mock_restore_storage_version: Mock,
) -> None:
    """Test exception during compile fires device_failed event."""
    entry = _make_entry()
    prebuild_dashboard.entries.async_all.return_value = [entry]
    mock_async_run_system_command.side_effect = OSError("Process failed")

    await prebuild_dashboard._async_prebuild_devices()

    calls = prebuild_dashboard.bus.async_fire.call_args_list
    assert len(calls) == 3

    assert calls[1].args[1]["status"] == "device_failed"
    assert calls[1].args[1]["error"] == "Unexpected error"

    assert calls[2].args[1]["succeeded"] == 0
    assert calls[2].args[1]["failed"] == 1

    mock_restore_storage_version.assert_called_once_with(entry, "2026.1.0")


@pytest.mark.asyncio
async def test_prebuild_mixed_success_and_failure(
    prebuild_dashboard: ESPHomeDashboard,
    mock_restore_storage_version: Mock,
) -> None:
    """Test multiple devices with mixed results report correct counts."""
    entry_ok = _make_entry(filename="ok.yaml", name="ok_device")
    entry_fail = _make_entry(filename="fail.yaml", name="fail_device")
    prebuild_dashboard.entries.async_all.return_value = [entry_ok, entry_fail]

    async def mock_compile(cmd: list[str]) -> tuple[int, bytes, bytes]:
        if "ok.yaml" in cmd:
            return (0, b"", b"")
        return (1, b"", b"build error")

    with patch(
        "esphome.dashboard.core.async_run_system_command",
        new_callable=AsyncMock,
        side_effect=mock_compile,
    ):
        await prebuild_dashboard._async_prebuild_devices()

    calls = prebuild_dashboard.bus.async_fire.call_args_list
    # started + device_done + device_failed + finished = 4
    assert len(calls) == 4

    finished = calls[3].args[1]
    assert finished["status"] == "finished"
    assert finished["succeeded"] == 1
    assert finished["failed"] == 1
    assert finished["total"] == 2


# -- auto_build flag integration tests --


@pytest.mark.asyncio
async def test_prebuild_runs_when_auto_build_enabled(
    prebuild_dashboard: ESPHomeDashboard,
    mock_async_run_system_command: AsyncMock,
) -> None:
    """Test pre-build is launched when settings.auto_build is True."""
    prebuild_dashboard.settings.auto_build = True
    entry = _make_entry()
    prebuild_dashboard.entries.async_all.return_value = [entry]
    mock_async_run_system_command.return_value = (0, b"", b"")

    await prebuild_dashboard._async_prebuild_devices()

    mock_async_run_system_command.assert_called_once()
    calls = prebuild_dashboard.bus.async_fire.call_args_list
    assert calls[0].args[1]["status"] == "started"


@pytest.mark.asyncio
async def test_prebuild_skipped_when_auto_build_disabled(
    prebuild_dashboard: ESPHomeDashboard,
    mock_async_run_system_command: AsyncMock,
) -> None:
    """Test _async_prebuild_devices is not called when settings.auto_build is False.

    This simulates the guard in async_run() by checking the flag before calling.
    """
    prebuild_dashboard.settings.auto_build = False
    entry = _make_entry()
    prebuild_dashboard.entries.async_all.return_value = [entry]
    mock_async_run_system_command.return_value = (0, b"", b"")

    # Replicate the guard logic from async_run()
    if prebuild_dashboard.settings.auto_build:
        await prebuild_dashboard._async_prebuild_devices()

    mock_async_run_system_command.assert_not_called()
    prebuild_dashboard.bus.async_fire.assert_not_called()


# -- _cleanup_old_firmware tests --


def test_cleanup_deletes_old_firmware_when_path_changes(
    mock_ext_storage_path: Mock, tmp_path: Path
) -> None:
    """Test old firmware binary is deleted when the new path differs."""
    old_bin = tmp_path / "old_firmware.bin"
    old_bin.write_bytes(b"\x00" * 100)

    # Storage now points to a new firmware path
    storage_file = mock_ext_storage_path.storage_file
    new_bin = tmp_path / "new_firmware.bin"
    storage_file.write_text(
        json.dumps(
            {
                "storage_version": 1,
                "name": "device",
                "firmware_bin_path": str(new_bin),
            }
        )
    )

    entry = Mock()
    entry.filename = "device.yaml"

    _cleanup_old_firmware(entry, old_bin)

    assert not old_bin.exists()


def test_cleanup_does_not_delete_when_paths_match(
    mock_ext_storage_path: Mock, tmp_path: Path
) -> None:
    """Test old firmware is NOT deleted when old and new paths are the same."""
    firmware_bin = tmp_path / "firmware.bin"
    firmware_bin.write_bytes(b"\x00" * 100)

    storage_file = mock_ext_storage_path.storage_file
    storage_file.write_text(
        json.dumps(
            {
                "storage_version": 1,
                "name": "device",
                "firmware_bin_path": str(firmware_bin),
            }
        )
    )

    entry = Mock()
    entry.filename = "device.yaml"

    _cleanup_old_firmware(entry, firmware_bin)

    assert firmware_bin.exists()


def test_cleanup_does_not_delete_on_failed_build(tmp_path: Path) -> None:
    """Test old firmware is NOT deleted when old_firmware_path is None."""
    entry = Mock()
    entry.filename = "device.yaml"

    # None means we couldn't read the storage before build — do nothing
    _cleanup_old_firmware(entry, None)


def test_cleanup_handles_missing_old_file(
    mock_ext_storage_path: Mock, tmp_path: Path
) -> None:
    """Test missing old firmware file does not raise."""
    old_bin = tmp_path / "nonexistent_firmware.bin"

    storage_file = mock_ext_storage_path.storage_file
    new_bin = tmp_path / "new_firmware.bin"
    storage_file.write_text(
        json.dumps(
            {
                "storage_version": 1,
                "name": "device",
                "firmware_bin_path": str(new_bin),
            }
        )
    )

    entry = Mock()
    entry.filename = "device.yaml"

    # Should not raise
    _cleanup_old_firmware(entry, old_bin)


def test_cleanup_handles_corrupt_storage(tmp_path: Path) -> None:
    """Test corrupt/missing storage JSON does not raise."""
    old_bin = tmp_path / "firmware.bin"
    old_bin.write_bytes(b"\x00" * 100)

    entry = Mock()
    entry.filename = "device.yaml"

    with patch(
        "esphome.dashboard.core.ext_storage_path",
        return_value=tmp_path / "nonexistent.json",
    ):
        # Should not raise — StorageJSON.load returns None for missing files
        _cleanup_old_firmware(entry, old_bin)

    # Old file should still exist since we couldn't read the new path
    assert old_bin.exists()


# -- Pre-build loop firmware cleanup integration tests --


@pytest.mark.asyncio
async def test_prebuild_cleans_up_old_firmware_on_success(
    prebuild_dashboard: ESPHomeDashboard,
    mock_async_run_system_command: AsyncMock,
    mock_storage_json_load: Mock,
    tmp_path: Path,
) -> None:
    """Test successful build triggers old firmware cleanup."""
    from esphome.storage_json import StorageJSON

    old_bin = tmp_path / "old_firmware.bin"
    old_bin.write_bytes(b"\x00" * 100)
    new_bin = tmp_path / "new_firmware.bin"

    entry = _make_entry()
    prebuild_dashboard.entries.async_all.return_value = [entry]
    mock_async_run_system_command.return_value = (0, b"", b"")

    # Build mock StorageJSON objects for old and new firmware paths
    old_storage = Mock(spec=StorageJSON)
    old_storage.firmware_bin_path = old_bin
    new_storage = Mock(spec=StorageJSON)
    new_storage.firmware_bin_path = new_bin

    # First call (before compile) returns old storage,
    # second call (inside _cleanup_old_firmware) returns new storage
    mock_storage_json_load.side_effect = [old_storage, new_storage]

    await prebuild_dashboard._async_prebuild_devices()

    assert not old_bin.exists()


@pytest.mark.asyncio
async def test_prebuild_does_not_cleanup_on_failure(
    prebuild_dashboard: ESPHomeDashboard,
    mock_async_run_system_command: AsyncMock,
    mock_storage_json_load: Mock,
    mock_restore_storage_version: Mock,
    tmp_path: Path,
) -> None:
    """Test failed build does NOT trigger firmware cleanup."""
    from esphome.storage_json import StorageJSON

    old_bin = tmp_path / "old_firmware.bin"
    old_bin.write_bytes(b"\x00" * 100)

    entry = _make_entry()
    prebuild_dashboard.entries.async_all.return_value = [entry]
    mock_async_run_system_command.return_value = (1, b"", b"build error")

    old_storage = Mock(spec=StorageJSON)
    old_storage.firmware_bin_path = old_bin
    mock_storage_json_load.return_value = old_storage

    await prebuild_dashboard._async_prebuild_devices()

    # Old firmware should NOT be deleted on failure
    assert old_bin.exists()
