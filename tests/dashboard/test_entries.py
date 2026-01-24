"""Tests for dashboard entries Path-related functionality."""

from __future__ import annotations

import os
from pathlib import Path
import tempfile
from unittest.mock import Mock

import pytest

from esphome.core import CORE
from esphome.dashboard.const import DashboardEvent
from esphome.dashboard.entries import DashboardEntries, DashboardEntry


def create_cache_key() -> tuple[int, int, float, int]:
    """Helper to create a valid DashboardCacheKeyType."""
    return (0, 0, 0.0, 0)


@pytest.fixture(autouse=True)
def setup_core():
    """Set up CORE for testing."""
    with tempfile.TemporaryDirectory() as tmpdir:
        CORE.config_path = Path(tmpdir) / "test.yaml"
        yield
        CORE.reset()


def test_dashboard_entry_path_initialization() -> None:
    """Test DashboardEntry initializes with path correctly."""
    test_path = Path("/test/config/device.yaml")
    cache_key = create_cache_key()

    entry = DashboardEntry(test_path, cache_key)

    assert entry.path == test_path
    assert entry.cache_key == cache_key


def test_dashboard_entry_path_with_absolute_path() -> None:
    """Test DashboardEntry handles absolute paths."""
    # Use a truly absolute path for the platform
    test_path = Path.cwd() / "absolute" / "path" / "to" / "config.yaml"
    cache_key = create_cache_key()

    entry = DashboardEntry(test_path, cache_key)

    assert entry.path == test_path
    assert entry.path.is_absolute()


def test_dashboard_entry_path_with_relative_path() -> None:
    """Test DashboardEntry handles relative paths."""
    test_path = Path("configs/device.yaml")
    cache_key = create_cache_key()

    entry = DashboardEntry(test_path, cache_key)

    assert entry.path == test_path
    assert not entry.path.is_absolute()


@pytest.mark.asyncio
async def test_dashboard_entries_get_by_path(
    dashboard_entries: DashboardEntries, tmp_path: Path
) -> None:
    """Test getting entry by path."""
    # Create a test file
    test_file = tmp_path / "device.yaml"
    test_file.write_text("test config")

    # Update entries to load the file
    await dashboard_entries.async_update_entries()

    # Verify the entry was loaded
    all_entries = dashboard_entries.async_all()
    assert len(all_entries) == 1
    entry = all_entries[0]
    assert entry.path == test_file

    # Also verify get() works with Path
    result = dashboard_entries.get(test_file)
    assert result == entry


@pytest.mark.asyncio
async def test_dashboard_entries_get_nonexistent_path(
    dashboard_entries: DashboardEntries,
) -> None:
    """Test getting non-existent entry returns None."""
    result = dashboard_entries.get("/nonexistent/path.yaml")
    assert result is None


@pytest.mark.asyncio
async def test_dashboard_entries_path_normalization(
    dashboard_entries: DashboardEntries, tmp_path: Path
) -> None:
    """Test that paths are handled consistently."""
    # Create a test file
    test_file = tmp_path / "device.yaml"
    test_file.write_text("test config")

    # Update entries to load the file
    await dashboard_entries.async_update_entries()

    # Get the entry by path
    result = dashboard_entries.get(test_file)
    assert result is not None


@pytest.mark.asyncio
async def test_dashboard_entries_path_with_spaces(
    dashboard_entries: DashboardEntries, tmp_path: Path
) -> None:
    """Test handling paths with spaces."""
    # Create a test file with spaces in name
    test_file = tmp_path / "my device.yaml"
    test_file.write_text("test config")

    # Update entries to load the file
    await dashboard_entries.async_update_entries()

    # Get the entry by path
    result = dashboard_entries.get(test_file)
    assert result is not None
    assert result.path == test_file


@pytest.mark.asyncio
async def test_dashboard_entries_path_with_special_chars(
    dashboard_entries: DashboardEntries, tmp_path: Path
) -> None:
    """Test handling paths with special characters."""
    # Create a test file with special characters
    test_file = tmp_path / "device-01_test.yaml"
    test_file.write_text("test config")

    # Update entries to load the file
    await dashboard_entries.async_update_entries()

    # Get the entry by path
    result = dashboard_entries.get(test_file)
    assert result is not None


def test_dashboard_entries_windows_path() -> None:
    """Test handling Windows-style paths."""
    test_path = Path(r"C:\Users\test\esphome\device.yaml")
    cache_key = create_cache_key()

    entry = DashboardEntry(test_path, cache_key)

    assert entry.path == test_path


@pytest.mark.asyncio
async def test_dashboard_entries_path_to_cache_key_mapping(
    dashboard_entries: DashboardEntries, tmp_path: Path
) -> None:
    """Test internal entries storage with paths and cache keys."""
    # Create test files
    file1 = tmp_path / "device1.yaml"
    file2 = tmp_path / "device2.yaml"
    file1.write_text("test config 1")
    file2.write_text("test config 2")

    # Update entries to load the files
    await dashboard_entries.async_update_entries()

    # Get entries and verify they have different cache keys
    entry1 = dashboard_entries.get(file1)
    entry2 = dashboard_entries.get(file2)

    assert entry1 is not None
    assert entry2 is not None
    assert entry1.cache_key != entry2.cache_key


def test_dashboard_entry_path_property() -> None:
    """Test that path property returns expected value."""
    test_path = Path("/test/config/device.yaml")
    entry = DashboardEntry(test_path, create_cache_key())

    assert entry.path == test_path
    assert isinstance(entry.path, Path)


@pytest.mark.asyncio
async def test_dashboard_entries_all_returns_entries_with_paths(
    dashboard_entries: DashboardEntries, tmp_path: Path
) -> None:
    """Test that all() returns entries with their paths intact."""
    # Create test files
    files = [
        tmp_path / "device1.yaml",
        tmp_path / "device2.yaml",
        tmp_path / "device3.yaml",
    ]

    for file in files:
        file.write_text("test config")

    # Update entries to load the files
    await dashboard_entries.async_update_entries()

    all_entries = dashboard_entries.async_all()

    assert len(all_entries) == len(files)
    retrieved_paths = [entry.path for entry in all_entries]
    assert set(retrieved_paths) == set(files)


@pytest.mark.asyncio
async def test_async_update_entries_removed_path(
    dashboard_entries: DashboardEntries, mock_dashboard: Mock, tmp_path: Path
) -> None:
    """Test that removed files trigger ENTRY_REMOVED event."""

    # Create a test file
    test_file = tmp_path / "device.yaml"
    test_file.write_text("test config")

    # First update to add the entry
    await dashboard_entries.async_update_entries()

    # Verify entry was added
    all_entries = dashboard_entries.async_all()
    assert len(all_entries) == 1
    entry = all_entries[0]

    # Delete the file
    test_file.unlink()

    # Second update to detect removal
    await dashboard_entries.async_update_entries()

    # Verify entry was removed
    all_entries = dashboard_entries.async_all()
    assert len(all_entries) == 0

    # Verify ENTRY_REMOVED event was fired
    mock_dashboard.bus.async_fire.assert_any_call(
        DashboardEvent.ENTRY_REMOVED, {"entry": entry}
    )


@pytest.mark.asyncio
async def test_async_update_entries_updated_path(
    dashboard_entries: DashboardEntries, mock_dashboard: Mock, tmp_path: Path
) -> None:
    """Test that modified files trigger ENTRY_UPDATED event."""

    # Create a test file
    test_file = tmp_path / "device.yaml"
    test_file.write_text("test config")

    # First update to add the entry
    await dashboard_entries.async_update_entries()

    # Verify entry was added
    all_entries = dashboard_entries.async_all()
    assert len(all_entries) == 1
    entry = all_entries[0]
    original_cache_key = entry.cache_key

    # Modify the file to change its mtime
    test_file.write_text("updated config")
    # Explicitly change the mtime to ensure it's different
    stat = test_file.stat()
    os.utime(test_file, (stat.st_atime, stat.st_mtime + 1))

    # Second update to detect modification
    await dashboard_entries.async_update_entries()

    # Verify entry is still there with updated cache key
    all_entries = dashboard_entries.async_all()
    assert len(all_entries) == 1
    updated_entry = all_entries[0]
    assert updated_entry == entry  # Same entry object
    assert updated_entry.cache_key != original_cache_key  # But cache key updated

    # Verify ENTRY_UPDATED event was fired
    mock_dashboard.bus.async_fire.assert_any_call(
        DashboardEvent.ENTRY_UPDATED, {"entry": entry}
    )
