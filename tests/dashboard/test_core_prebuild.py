"""Tests for the dashboard pre-build firmware feature."""

from __future__ import annotations

from collections.abc import Generator
import json
from pathlib import Path
from unittest.mock import AsyncMock, Mock, patch

import pytest

from esphome.dashboard.const import DashboardEvent
from esphome.dashboard.core import ESPHomeDashboard, _restore_storage_version


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
def prebuild_dashboard() -> ESPHomeDashboard:
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
