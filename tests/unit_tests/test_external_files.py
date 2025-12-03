"""Tests for external_files.py functions."""

from pathlib import Path
import time
from unittest.mock import MagicMock, patch

import pytest
import requests

from esphome import external_files
from esphome.config_validation import Invalid
from esphome.core import CORE, TimePeriod


def test_compute_local_file_dir(setup_core: Path) -> None:
    """Test compute_local_file_dir creates and returns correct path."""
    domain = "font"

    result = external_files.compute_local_file_dir(domain)

    assert isinstance(result, Path)
    assert result == Path(CORE.data_dir) / domain
    assert result.exists()
    assert result.is_dir()


def test_compute_local_file_dir_nested(setup_core: Path) -> None:
    """Test compute_local_file_dir works with nested domains."""
    domain = "images/icons"

    result = external_files.compute_local_file_dir(domain)

    assert result == Path(CORE.data_dir) / "images" / "icons"
    assert result.exists()
    assert result.is_dir()


def test_is_file_recent_with_recent_file(setup_core: Path) -> None:
    """Test is_file_recent returns True for recently created file."""
    test_file = setup_core / "recent.txt"
    test_file.write_text("content")

    refresh = TimePeriod(seconds=3600)

    result = external_files.is_file_recent(test_file, refresh)

    assert result is True


def test_is_file_recent_with_old_file(setup_core: Path) -> None:
    """Test is_file_recent returns False for old file."""
    test_file = setup_core / "old.txt"
    test_file.write_text("content")

    old_time = time.time() - 7200
    mock_stat = MagicMock()
    mock_stat.st_ctime = old_time

    with patch.object(Path, "stat", return_value=mock_stat):
        refresh = TimePeriod(seconds=3600)

        result = external_files.is_file_recent(test_file, refresh)

        assert result is False


def test_is_file_recent_nonexistent_file(setup_core: Path) -> None:
    """Test is_file_recent returns False for non-existent file."""
    test_file = setup_core / "nonexistent.txt"
    refresh = TimePeriod(seconds=3600)

    result = external_files.is_file_recent(test_file, refresh)

    assert result is False


def test_is_file_recent_with_zero_refresh(setup_core: Path) -> None:
    """Test is_file_recent with zero refresh period returns False."""
    test_file = setup_core / "test.txt"
    test_file.write_text("content")

    # Mock stat to return a time 10 seconds ago
    mock_stat = MagicMock()
    mock_stat.st_ctime = time.time() - 10
    with patch.object(Path, "stat", return_value=mock_stat):
        refresh = TimePeriod(seconds=0)
        result = external_files.is_file_recent(test_file, refresh)
        assert result is False


@patch("esphome.external_files.requests.head")
def test_has_remote_file_changed_not_modified(
    mock_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed returns False when file not modified."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_head.return_value = mock_response

    url = "https://example.com/file.txt"
    result = external_files.has_remote_file_changed(url, test_file)

    assert result is False
    mock_head.assert_called_once()

    call_args = mock_head.call_args
    headers = call_args[1]["headers"]
    assert external_files.IF_MODIFIED_SINCE in headers
    assert external_files.CACHE_CONTROL in headers


@patch("esphome.external_files.requests.head")
def test_has_remote_file_changed_modified(
    mock_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed returns True when file modified."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_response = MagicMock()
    mock_response.status_code = 200
    mock_head.return_value = mock_response

    url = "https://example.com/file.txt"
    result = external_files.has_remote_file_changed(url, test_file)

    assert result is True


def test_has_remote_file_changed_no_local_file(setup_core: Path) -> None:
    """Test has_remote_file_changed returns True when local file doesn't exist."""
    test_file = setup_core / "nonexistent.txt"

    url = "https://example.com/file.txt"
    result = external_files.has_remote_file_changed(url, test_file)

    assert result is True


@patch("esphome.external_files.requests.head")
def test_has_remote_file_changed_network_error(
    mock_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed handles network errors gracefully."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_head.side_effect = requests.exceptions.RequestException("Network error")

    url = "https://example.com/file.txt"

    with pytest.raises(Invalid, match="Could not check if.*Network error"):
        external_files.has_remote_file_changed(url, test_file)


@patch("esphome.external_files.requests.head")
def test_has_remote_file_changed_timeout(
    mock_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed respects timeout."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_head.return_value = mock_response

    url = "https://example.com/file.txt"
    external_files.has_remote_file_changed(url, test_file)

    call_args = mock_head.call_args
    assert call_args[1]["timeout"] == external_files.NETWORK_TIMEOUT


def test_compute_local_file_dir_creates_parent_dirs(setup_core: Path) -> None:
    """Test compute_local_file_dir creates parent directories."""
    domain = "level1/level2/level3/level4"

    result = external_files.compute_local_file_dir(domain)

    assert result.exists()
    assert result.is_dir()
    assert result.parent.name == "level3"
    assert result.parent.parent.name == "level2"
    assert result.parent.parent.parent.name == "level1"


def test_is_file_recent_handles_float_seconds(setup_core: Path) -> None:
    """Test is_file_recent works with float seconds in TimePeriod."""
    test_file = setup_core / "test.txt"
    test_file.write_text("content")

    refresh = TimePeriod(seconds=3600.5)

    result = external_files.is_file_recent(test_file, refresh)

    assert result is True
