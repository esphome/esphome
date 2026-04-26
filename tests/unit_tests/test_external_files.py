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
    mock_response.headers = {}
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
    mock_response.headers = {}
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
    """Test has_remote_file_changed returns False on network error when file is cached."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_head.side_effect = requests.exceptions.RequestException("Network error")

    url = "https://example.com/file.txt"
    result = external_files.has_remote_file_changed(url, test_file)

    assert result is False


@patch("esphome.external_files.requests.head")
def test_has_remote_file_changed_timeout(
    mock_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed respects timeout."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_response.headers = {}
    mock_head.return_value = mock_response

    url = "https://example.com/file.txt"
    external_files.has_remote_file_changed(url, test_file)

    call_args = mock_head.call_args
    assert call_args[1]["timeout"] == external_files.NETWORK_TIMEOUT


@patch("esphome.external_files.requests.head")
def test_has_remote_file_changed_uses_etag(
    mock_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed sends If-None-Match when ETag is cached."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")
    external_files._etag_sidecar_path(test_file).write_text('"abc123"')

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_response.headers = {}
    mock_head.return_value = mock_response

    url = "https://example.com/file.txt"
    result = external_files.has_remote_file_changed(url, test_file)

    assert result is False
    headers = mock_head.call_args[1]["headers"]
    assert headers[external_files.IF_NONE_MATCH] == '"abc123"'


@patch("esphome.external_files.requests.head")
def test_has_remote_file_changed_no_etag_no_if_none_match(
    mock_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed omits If-None-Match when no ETag is cached."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_response.headers = {}
    mock_head.return_value = mock_response

    url = "https://example.com/file.txt"
    external_files.has_remote_file_changed(url, test_file)

    headers = mock_head.call_args[1]["headers"]
    assert external_files.IF_NONE_MATCH not in headers


@patch("esphome.external_files.requests.head")
def test_has_remote_file_changed_refreshes_etag_on_304(
    mock_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed updates the cached ETag when the 304 sends a new one."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")
    external_files._etag_sidecar_path(test_file).write_text('"old"')

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_response.headers = {external_files.ETAG: '"new"'}
    mock_head.return_value = mock_response

    url = "https://example.com/file.txt"
    external_files.has_remote_file_changed(url, test_file)

    assert external_files._etag_sidecar_path(test_file).read_text() == '"new"'


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


@patch("esphome.external_files.requests.get")
@patch("esphome.external_files.has_remote_file_changed")
def test_download_content_with_network_error_uses_cache(
    mock_has_changed: MagicMock, mock_get: MagicMock, setup_core: Path
) -> None:
    """Test download_content uses cached file when network fails."""
    test_file = setup_core / "cached.txt"
    cached_content = b"cached content"
    test_file.write_bytes(cached_content)

    # Simulate file has changed, so it tries to download
    mock_has_changed.return_value = True
    mock_get.side_effect = requests.exceptions.RequestException("Network error")

    url = "https://example.com/file.txt"
    result = external_files.download_content(url, test_file)

    assert result == cached_content


@patch("esphome.external_files.requests.get")
@patch("esphome.external_files.has_remote_file_changed")
def test_download_content_with_network_error_no_cache_fails(
    mock_has_changed: MagicMock, mock_get: MagicMock, setup_core: Path
) -> None:
    """Test download_content raises error when network fails and no cache exists."""
    test_file = setup_core / "nonexistent.txt"

    # Simulate file has changed (doesn't exist), so it tries to download
    mock_has_changed.return_value = True
    mock_get.side_effect = requests.exceptions.RequestException("Network error")

    url = "https://example.com/file.txt"

    with pytest.raises(Invalid, match="Could not download from.*Network error"):
        external_files.download_content(url, test_file)


@patch("esphome.external_files.requests.get")
@patch("esphome.external_files.has_remote_file_changed")
def test_download_content_skip_external_update_uses_cache(
    mock_has_changed: MagicMock,
    mock_get: MagicMock,
    setup_core: Path,
) -> None:
    """Test download_content skips network checks when CORE.skip_external_update is set."""
    test_file = setup_core / "cached.txt"
    cached_content = b"cached content"
    test_file.write_bytes(cached_content)

    CORE.skip_external_update = True
    url = "https://example.com/file.txt"
    result = external_files.download_content(url, test_file)

    assert result == cached_content
    mock_has_changed.assert_not_called()
    mock_get.assert_not_called()


@patch("esphome.external_files.requests.get")
@patch("esphome.external_files.has_remote_file_changed")
def test_download_content_skip_external_update_downloads_when_missing(
    mock_has_changed: MagicMock,
    mock_get: MagicMock,
    setup_core: Path,
) -> None:
    """Test download_content still downloads when file is missing, even with skip_external_update."""
    test_file = setup_core / "missing.txt"
    new_content = b"fresh content"

    mock_has_changed.return_value = True
    mock_response = MagicMock()
    mock_response.content = new_content
    mock_response.headers = {}
    mock_response.raise_for_status = MagicMock()
    mock_get.return_value = mock_response

    CORE.skip_external_update = True
    url = "https://example.com/file.txt"
    result = external_files.download_content(url, test_file)

    assert result == new_content
    assert test_file.read_bytes() == new_content


@patch("esphome.external_files.requests.get")
@patch("esphome.external_files.has_remote_file_changed")
def test_download_content_saves_etag(
    mock_has_changed: MagicMock,
    mock_get: MagicMock,
    setup_core: Path,
) -> None:
    """Test download_content writes the ETag sidecar after a successful download."""
    test_file = setup_core / "fresh.txt"
    new_content = b"fresh content"

    mock_has_changed.return_value = True
    mock_response = MagicMock()
    mock_response.content = new_content
    mock_response.headers = {external_files.ETAG: '"deadbeef"'}
    mock_response.raise_for_status = MagicMock()
    mock_get.return_value = mock_response

    url = "https://example.com/file.txt"
    external_files.download_content(url, test_file)

    assert external_files._etag_sidecar_path(test_file).read_text() == '"deadbeef"'


@patch("esphome.external_files.requests.get")
@patch("esphome.external_files.has_remote_file_changed")
def test_download_content_atomic_write_no_partial_on_failure(
    mock_has_changed: MagicMock,
    mock_get: MagicMock,
    setup_core: Path,
) -> None:
    """Test download_content does not corrupt the existing file if the write step fails."""
    test_file = setup_core / "cached.txt"
    original_content = b"original content"
    test_file.write_bytes(original_content)

    mock_has_changed.return_value = True
    mock_response = MagicMock()
    # Accessing .content raises, simulating a streaming/decode failure
    type(mock_response).content = property(
        lambda self: (_ for _ in ()).throw(OSError("disk full"))
    )
    mock_response.raise_for_status = MagicMock()
    mock_get.return_value = mock_response

    url = "https://example.com/file.txt"
    with pytest.raises(OSError, match="disk full"):
        external_files.download_content(url, test_file)

    # Original file is untouched (atomic rename never happened)
    assert test_file.read_bytes() == original_content
    # No leftover temp files from tempfile.NamedTemporaryFile
    leftover_tmps = list(setup_core.glob("tmp*"))
    assert leftover_tmps == []


@patch("esphome.external_files.download_content")
def test_download_content_many_empty_is_noop(
    mock_download: MagicMock, setup_core: Path
) -> None:
    """Empty input shouldn't spin up a thread pool or call download_content."""
    external_files.download_content_many([])
    mock_download.assert_not_called()


@patch("esphome.external_files.download_content")
def test_download_content_many_single_item_avoids_pool(
    mock_download: MagicMock, setup_core: Path
) -> None:
    """A single item should be downloaded inline (no thread pool overhead)."""
    item = ("https://example.com/file.txt", setup_core / "f.txt")
    external_files.download_content_many([item])
    mock_download.assert_called_once_with(
        item[0], item[1], external_files.NETWORK_TIMEOUT
    )


@patch("esphome.external_files.download_content")
def test_download_content_many_runs_in_parallel(
    mock_download: MagicMock, setup_core: Path
) -> None:
    """Multiple items should run concurrently — total wall time ≈ max latency."""
    import threading

    barrier = threading.Barrier(3)

    def slow_download(url: str, path: Path, timeout: int) -> bytes:
        # If calls were serial this would deadlock (third caller never arrives
        # while the first is blocked at the barrier).
        barrier.wait(timeout=2.0)
        return b""

    mock_download.side_effect = slow_download
    items = [
        ("https://example.com/a", setup_core / "a"),
        ("https://example.com/b", setup_core / "b"),
        ("https://example.com/c", setup_core / "c"),
    ]
    external_files.download_content_many(items, max_workers=4)
    assert mock_download.call_count == 3


@patch("esphome.external_files.download_content")
def test_download_content_many_propagates_errors(
    mock_download: MagicMock, setup_core: Path
) -> None:
    """An exception from any worker must propagate out of download_content_many."""

    def fake_download(url: str, path: Path, timeout: int) -> bytes:
        if url.endswith("bad"):
            raise Invalid(f"could not download {url}")
        return b""

    mock_download.side_effect = fake_download
    items = [
        ("https://example.com/ok", setup_core / "ok"),
        ("https://example.com/bad", setup_core / "bad"),
    ]
    with pytest.raises(Invalid, match="could not download"):
        external_files.download_content_many(items)
