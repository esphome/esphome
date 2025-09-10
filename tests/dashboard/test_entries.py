"""Tests for dashboard entries Path-related functionality."""

from __future__ import annotations

from pathlib import Path
import tempfile
from unittest.mock import MagicMock

import pytest
import pytest_asyncio

from esphome.core import CORE
from esphome.dashboard.entries import DashboardEntries, DashboardEntry


def create_cache_key() -> tuple[int, int, float, int]:
    """Helper to create a valid DashboardCacheKeyType."""
    return (0, 0, 0.0, 0)


@pytest.fixture(autouse=True)
def setup_core():
    """Set up CORE for testing."""
    with tempfile.TemporaryDirectory() as tmpdir:
        CORE.config_path = str(Path(tmpdir) / "test.yaml")
        yield
        CORE.reset()


@pytest.fixture
def mock_settings() -> MagicMock:
    """Create mock dashboard settings."""
    settings = MagicMock()
    settings.config_dir = "/test/config"
    settings.absolute_config_dir = Path("/test/config")
    return settings


@pytest_asyncio.fixture
async def dashboard_entries(mock_settings: MagicMock) -> DashboardEntries:
    """Create a DashboardEntries instance for testing."""
    return DashboardEntries(mock_settings)


def test_dashboard_entry_path_initialization() -> None:
    """Test DashboardEntry initializes with path correctly."""
    test_path = "/test/config/device.yaml"
    cache_key = create_cache_key()

    entry = DashboardEntry(test_path, cache_key)

    assert entry.path == test_path
    assert entry.cache_key == cache_key


def test_dashboard_entry_path_with_absolute_path() -> None:
    """Test DashboardEntry handles absolute paths."""
    # Use a truly absolute path for the platform
    test_path = Path.cwd() / "absolute" / "path" / "to" / "config.yaml"
    cache_key = create_cache_key()

    entry = DashboardEntry(str(test_path), cache_key)

    assert entry.path == str(test_path)
    assert Path(entry.path).is_absolute()


def test_dashboard_entry_path_with_relative_path() -> None:
    """Test DashboardEntry handles relative paths."""
    test_path = "configs/device.yaml"
    cache_key = create_cache_key()

    entry = DashboardEntry(test_path, cache_key)

    assert entry.path == test_path
    assert not Path(entry.path).is_absolute()


@pytest.mark.asyncio
async def test_dashboard_entries_get_by_path(
    dashboard_entries: DashboardEntries,
) -> None:
    """Test getting entry by path."""
    test_path = "/test/config/device.yaml"
    entry = DashboardEntry(test_path, create_cache_key())

    dashboard_entries._entries[test_path] = entry

    result = dashboard_entries.get(test_path)
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
    dashboard_entries: DashboardEntries,
) -> None:
    """Test that paths are handled consistently."""
    path1 = "/test/config/device.yaml"

    entry = DashboardEntry(path1, create_cache_key())
    dashboard_entries._entries[path1] = entry

    result = dashboard_entries.get(path1)
    assert result == entry


@pytest.mark.asyncio
async def test_dashboard_entries_path_with_spaces(
    dashboard_entries: DashboardEntries,
) -> None:
    """Test handling paths with spaces."""
    test_path = "/test/config/my device.yaml"
    entry = DashboardEntry(test_path, create_cache_key())

    dashboard_entries._entries[test_path] = entry

    result = dashboard_entries.get(test_path)
    assert result == entry
    assert result.path == test_path


@pytest.mark.asyncio
async def test_dashboard_entries_path_with_special_chars(
    dashboard_entries: DashboardEntries,
) -> None:
    """Test handling paths with special characters."""
    test_path = "/test/config/device-01_test.yaml"
    entry = DashboardEntry(test_path, create_cache_key())

    dashboard_entries._entries[test_path] = entry

    result = dashboard_entries.get(test_path)
    assert result == entry


def test_dashboard_entries_windows_path() -> None:
    """Test handling Windows-style paths."""
    test_path = r"C:\Users\test\esphome\device.yaml"
    cache_key = create_cache_key()

    entry = DashboardEntry(test_path, cache_key)

    assert entry.path == test_path


@pytest.mark.asyncio
async def test_dashboard_entries_path_to_cache_key_mapping(
    dashboard_entries: DashboardEntries,
) -> None:
    """Test internal entries storage with paths and cache keys."""
    path1 = "/test/config/device1.yaml"
    path2 = "/test/config/device2.yaml"

    entry1 = DashboardEntry(path1, create_cache_key())
    entry2 = DashboardEntry(path2, (1, 1, 1.0, 1))

    dashboard_entries._entries[path1] = entry1
    dashboard_entries._entries[path2] = entry2

    assert path1 in dashboard_entries._entries
    assert path2 in dashboard_entries._entries
    assert dashboard_entries._entries[path1].cache_key == create_cache_key()
    assert dashboard_entries._entries[path2].cache_key == (1, 1, 1.0, 1)


def test_dashboard_entry_path_property() -> None:
    """Test that path property returns expected value."""
    test_path = "/test/config/device.yaml"
    entry = DashboardEntry(test_path, create_cache_key())

    assert entry.path == test_path
    assert isinstance(entry.path, str)


@pytest.mark.asyncio
async def test_dashboard_entries_all_returns_entries_with_paths(
    dashboard_entries: DashboardEntries,
) -> None:
    """Test that all() returns entries with their paths intact."""
    paths = [
        "/test/config/device1.yaml",
        "/test/config/device2.yaml",
        "/test/config/subfolder/device3.yaml",
    ]

    for path in paths:
        entry = DashboardEntry(path, create_cache_key())
        dashboard_entries._entries[path] = entry

    all_entries = dashboard_entries.async_all()

    assert len(all_entries) == len(paths)
    retrieved_paths = [entry.path for entry in all_entries]
    assert set(retrieved_paths) == set(paths)
