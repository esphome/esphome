"""Tests for external_files.py functions."""

import os
from pathlib import Path
import time
from typing import Any
from unittest.mock import MagicMock, call, patch

import pytest
import requests

from esphome import external_files
from esphome.config_validation import Invalid, MultipleInvalid
from esphome.core import CORE, EsphomeError, TimePeriod


def _seed_etag(cache_file: Path, etag: str) -> Path:
    """Write an ETag sidecar with its mtime synced to the cache file's mtime,
    matching the invariant that `_write_etag` enforces in production.
    """
    sidecar = external_files._etag_sidecar_path(cache_file)
    sidecar.write_text(etag)
    file_mtime = int(cache_file.stat().st_mtime)
    os.utime(sidecar, (file_mtime, file_mtime))
    return sidecar


@pytest.fixture
def mock_requests_head() -> MagicMock:
    """Patch `requests.head` so the conditional HEAD-request validator can
    be tested without doing real HTTP. Patched on the requests module
    because external_files imports it lazily inside the function.
    """
    with patch("requests.head") as m:
        yield m


@pytest.fixture
def mock_requests_get() -> MagicMock:
    """Patch `requests.get` so the download path can be tested without
    doing real HTTP. Patched on the requests module because
    external_files imports it lazily inside the function.
    """
    with patch("requests.get") as m:
        yield m


@pytest.fixture
def mock_has_remote_file_changed() -> MagicMock:
    """Patch `external_files.has_remote_file_changed` so download tests can
    control the conditional check independently from the GET path.
    """
    with patch("esphome.external_files.has_remote_file_changed") as m:
        yield m


@pytest.fixture
def mock_write_file() -> MagicMock:
    """Patch `external_files.write_file` so atomic-write failures can be
    injected without involving the real filesystem helper.
    """
    with patch("esphome.external_files.write_file") as m:
        yield m


@pytest.fixture
def mock_download_content() -> MagicMock:
    """Patch `external_files.download_content` for tests that exercise the
    parallel batch helper without doing real I/O.
    """
    with patch("esphome.external_files.download_content") as m:
        yield m


@pytest.fixture
def mock_download_content_many() -> MagicMock:
    """Patch `external_files.download_content_many` for tests that exercise
    the URL-collection helper without dispatching to the thread pool.
    """
    with patch("esphome.external_files.download_content_many") as m:
        yield m


@pytest.fixture
def mock_retry_sleep() -> MagicMock:
    """Patch the retry backoff sleep (process-wide; net_retry.time is the
    global module) so transient-error tests don't really wait 2s/4s.
    """
    with patch("esphome.net_retry.time.sleep") as m:
        yield m


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
    mock_stat.st_mtime = old_time

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
    mock_stat.st_mtime = time.time() - 10
    with patch.object(Path, "stat", return_value=mock_stat):
        refresh = TimePeriod(seconds=0)
        result = external_files.is_file_recent(test_file, refresh)
        assert result is False


def test_has_remote_file_changed_not_modified(
    mock_requests_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed returns False when file not modified."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_response.headers = {}
    mock_requests_head.return_value = mock_response

    url = "https://example.com/file.txt"
    result = external_files.has_remote_file_changed(url, test_file)

    assert result is False
    mock_requests_head.assert_called_once()

    call_args = mock_requests_head.call_args
    headers = call_args[1]["headers"]
    assert external_files.IF_MODIFIED_SINCE in headers
    assert external_files.CACHE_CONTROL in headers


def test_has_remote_file_changed_modified(
    mock_requests_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed returns True when file modified."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_response = MagicMock()
    mock_response.status_code = 200
    mock_response.headers = {}
    mock_requests_head.return_value = mock_response

    url = "https://example.com/file.txt"
    result = external_files.has_remote_file_changed(url, test_file)

    assert result is True


def test_has_remote_file_changed_no_local_file(setup_core: Path) -> None:
    """Test has_remote_file_changed returns True when local file doesn't exist."""
    test_file = setup_core / "nonexistent.txt"

    url = "https://example.com/file.txt"
    result = external_files.has_remote_file_changed(url, test_file)

    assert result is True


def test_has_remote_file_changed_network_error(
    mock_requests_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed returns False on network error when file is cached."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_requests_head.side_effect = requests.exceptions.RequestException(
        "Network error"
    )

    url = "https://example.com/file.txt"
    result = external_files.has_remote_file_changed(url, test_file)

    assert result is False


def test_has_remote_file_changed_timeout(
    mock_requests_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed respects timeout."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_response.headers = {}
    mock_requests_head.return_value = mock_response

    url = "https://example.com/file.txt"
    external_files.has_remote_file_changed(url, test_file)

    call_args = mock_requests_head.call_args
    assert call_args[1]["timeout"] == external_files.NETWORK_TIMEOUT


def test_has_remote_file_changed_uses_etag(
    mock_requests_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed sends If-None-Match when ETag is cached."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")
    _seed_etag(test_file, '"abc123"')

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_response.headers = {}
    mock_requests_head.return_value = mock_response

    url = "https://example.com/file.txt"
    result = external_files.has_remote_file_changed(url, test_file)

    assert result is False
    headers = mock_requests_head.call_args[1]["headers"]
    assert headers[external_files.IF_NONE_MATCH] == '"abc123"'


def test_has_remote_file_changed_no_etag_no_if_none_match(
    mock_requests_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed omits If-None-Match when no ETag is cached."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_response.headers = {}
    mock_requests_head.return_value = mock_response

    url = "https://example.com/file.txt"
    external_files.has_remote_file_changed(url, test_file)

    headers = mock_requests_head.call_args[1]["headers"]
    assert external_files.IF_NONE_MATCH not in headers


def test_has_remote_file_changed_refreshes_etag_on_304(
    mock_requests_head: MagicMock, setup_core: Path
) -> None:
    """Test has_remote_file_changed updates the cached ETag when the 304 sends a new one."""
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")
    _seed_etag(test_file, '"old"')

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_response.headers = {external_files.ETAG: '"new"'}
    mock_requests_head.return_value = mock_response

    url = "https://example.com/file.txt"
    external_files.has_remote_file_changed(url, test_file)

    assert external_files._etag_sidecar_path(test_file).read_text() == '"new"'


def test_has_remote_file_changed_ignores_etag_when_mtime_diverges(
    mock_requests_head: MagicMock, setup_core: Path
) -> None:
    """If the cache file was edited out-of-band (mtime no longer matches the
    sidecar's), the cached ETag must not be used -- it no longer describes the
    bytes on disk.
    """
    test_file = setup_core / "cached.txt"
    test_file.write_text("cached content")
    sidecar = _seed_etag(test_file, '"abc123"')

    # Simulate an out-of-band edit to the cache file -- mtime advances by a
    # full second (so it diverges at whole-second resolution) but the sidecar
    # is left untouched, so the recorded ETag is now stale.
    file_stat = test_file.stat()
    os.utime(test_file, (file_stat.st_atime, file_stat.st_mtime + 1))

    mock_response = MagicMock()
    mock_response.status_code = 304
    mock_response.headers = {}
    mock_requests_head.return_value = mock_response

    external_files.has_remote_file_changed("https://example.com/file.txt", test_file)

    headers = mock_requests_head.call_args[1]["headers"]
    assert external_files.IF_NONE_MATCH not in headers
    # Stale sidecar should be removed so future calls don't keep paying the
    # mtime-comparison cost on a known-bad sidecar.
    assert not sidecar.exists()


def test_download_content_pins_etag_mtime_to_file_mtime(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """After a successful download, the sidecar's mtime must equal the cache
    file's mtime so `_read_etag` accepts it on the next call.
    """
    test_file = setup_core / "fresh.txt"
    mock_has_remote_file_changed.return_value = True
    mock_response = MagicMock()
    mock_response.content = b"fresh content"
    mock_response.headers = {external_files.ETAG: '"deadbeef"'}
    mock_response.raise_for_status = MagicMock()
    mock_requests_get.return_value = mock_response

    external_files.download_content("https://example.com/file.txt", test_file)

    sidecar = external_files._etag_sidecar_path(test_file)
    assert int(sidecar.stat().st_mtime) == int(test_file.stat().st_mtime)


def test_write_etag_swallows_write_file_failure(
    mock_write_file: MagicMock, setup_core: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """If `write_file` raises, _write_etag must not propagate -- ETag
    persistence is best-effort and a failure here must not abort the
    surrounding download.
    """
    cache_file = setup_core / "cached.txt"
    cache_file.write_text("cached content")
    mock_write_file.side_effect = EsphomeError("disk full")

    with caplog.at_level("DEBUG", logger="esphome.external_files"):
        external_files._write_etag(cache_file, '"abc123"')

    assert "Could not save ETag" in caplog.text
    # Sidecar wasn't created, since write_file was mocked to fail before
    # reaching the os.utime step.
    assert not external_files._etag_sidecar_path(cache_file).exists()


def test_write_etag_swallows_utime_failure(
    setup_core: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """If `os.utime` raises while pinning the sidecar's mtime, _write_etag
    must not propagate. The sidecar is still written; if its mtime later
    fails to match the cache file, `_read_etag` will discard it on next
    read.
    """
    cache_file = setup_core / "cached.txt"
    cache_file.write_text("cached content")

    with (
        patch(
            "esphome.external_files.os.utime",
            side_effect=PermissionError("nope"),
        ),
        caplog.at_level("DEBUG", logger="esphome.external_files"),
    ):
        external_files._write_etag(cache_file, '"abc123"')

    assert "Could not sync ETag sidecar mtime" in caplog.text
    # write_file succeeded, so the sidecar exists with the new value even
    # though we couldn't pin its mtime.
    sidecar = external_files._etag_sidecar_path(cache_file)
    assert sidecar.exists()
    assert sidecar.read_text() == '"abc123"'


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


def test_download_content_with_network_error_uses_cache(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """Test download_content uses cached file when network fails."""
    test_file = setup_core / "cached.txt"
    cached_content = b"cached content"
    test_file.write_bytes(cached_content)

    # Simulate file has changed, so it tries to download
    mock_has_remote_file_changed.return_value = True
    mock_requests_get.side_effect = requests.exceptions.RequestException(
        "Network error"
    )

    url = "https://example.com/file.txt"
    result = external_files.download_content(url, test_file)

    assert result == cached_content


def test_download_content_with_network_error_no_cache_fails(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """Test download_content raises error when network fails and no cache exists."""
    test_file = setup_core / "nonexistent.txt"

    # Simulate file has changed (doesn't exist), so it tries to download
    mock_has_remote_file_changed.return_value = True
    mock_requests_get.side_effect = requests.exceptions.RequestException(
        "Network error"
    )

    url = "https://example.com/file.txt"

    with pytest.raises(Invalid, match="Could not download from.*Network error"):
        external_files.download_content(url, test_file)


class _BodyReadErrorResponse:
    """Stand-in for `requests.Response` whose `.content` raises on access.

    A small dedicated stub avoids mutating `MagicMock`'s class with a
    `property` (which would leak across every other MagicMock-based test
    in this file).
    """

    def __init__(self, exc: Exception) -> None:
        self._exc = exc
        self.headers: dict[str, str] = {}

    def raise_for_status(self) -> None:
        return None

    @property
    def content(self) -> bytes:
        raise self._exc


def test_download_content_with_body_read_error_uses_cache(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    mock_retry_sleep: MagicMock,
    setup_core: Path,
) -> None:
    """Body-read errors (chunked-decode/gzip-decode/mid-stream connection
    drop) raise RequestException subclasses on `.content` access, not from
    `requests.get` itself. They must follow the same fall-back-to-cache
    path as a connect-time failure.
    """
    test_file = setup_core / "cached.txt"
    cached_content = b"cached content"
    test_file.write_bytes(cached_content)

    mock_has_remote_file_changed.return_value = True
    mock_requests_get.return_value = _BodyReadErrorResponse(
        requests.exceptions.ChunkedEncodingError("body truncated")
    )

    result = external_files.download_content("https://example.com/file.txt", test_file)

    assert result == cached_content


def test_download_content_with_body_read_error_no_cache_fails(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    mock_retry_sleep: MagicMock,
    setup_core: Path,
) -> None:
    """A body-read failure with no cache available must surface as a
    cv.Invalid, same as a connect-time failure with no cache.
    """
    test_file = setup_core / "nonexistent.txt"

    mock_has_remote_file_changed.return_value = True
    mock_requests_get.return_value = _BodyReadErrorResponse(
        requests.exceptions.ChunkedEncodingError("body truncated")
    )

    with pytest.raises(Invalid, match="Could not download from.*body truncated"):
        external_files.download_content("https://example.com/file.txt", test_file)


def test_download_content_retries_transient_error_then_succeeds(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    mock_retry_sleep: MagicMock,
    setup_core: Path,
) -> None:
    """Transient failures (connection reset, timeout) are retried with 2s/4s
    backoff before giving up; a late success downloads normally."""
    test_file = setup_core / "downloads" / "file.txt"
    mock_has_remote_file_changed.return_value = True

    ok = MagicMock()
    ok.content = b"downloaded"
    ok.headers = {}
    mock_requests_get.side_effect = [
        requests.exceptions.ConnectionError("reset by peer"),
        requests.exceptions.Timeout("timed out"),
        ok,
    ]

    result = external_files.download_content("https://example.com/file.txt", test_file)

    assert result == b"downloaded"
    assert test_file.read_bytes() == b"downloaded"
    assert mock_retry_sleep.call_args_list == [call(2), call(4)]


def test_download_content_transient_error_exhausts_attempts(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    mock_retry_sleep: MagicMock,
    setup_core: Path,
) -> None:
    """A persistent transient failure gives up after three attempts and then
    follows the normal no-cache error path."""
    test_file = setup_core / "nonexistent.txt"
    mock_has_remote_file_changed.return_value = True
    mock_requests_get.side_effect = requests.exceptions.ConnectionError("reset by peer")

    with pytest.raises(Invalid, match="Could not download from.*reset by peer"):
        external_files.download_content("https://example.com/file.txt", test_file)

    assert mock_retry_sleep.call_args_list == [call(2), call(4)]


def test_download_content_non_transient_error_not_retried(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    mock_retry_sleep: MagicMock,
    setup_core: Path,
) -> None:
    """Permanent failures like a 404 fail on the first attempt."""
    test_file = setup_core / "nonexistent.txt"
    mock_has_remote_file_changed.return_value = True

    response = MagicMock()
    response.status_code = 404
    mock_requests_get.side_effect = requests.exceptions.HTTPError(
        "404 Client Error", response=response
    )

    with pytest.raises(Invalid, match="Could not download from.*404"):
        external_files.download_content("https://example.com/file.txt", test_file)

    assert mock_requests_get.call_count == 1
    mock_retry_sleep.assert_not_called()


def test_download_content_retries_body_read_error(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    mock_retry_sleep: MagicMock,
    setup_core: Path,
) -> None:
    """Mid-stream failures surfacing from `.content` are retried too."""
    test_file = setup_core / "downloads" / "file.txt"
    mock_has_remote_file_changed.return_value = True

    ok = MagicMock()
    ok.content = b"downloaded"
    ok.headers = {}
    mock_requests_get.side_effect = [
        _BodyReadErrorResponse(
            requests.exceptions.ChunkedEncodingError("body truncated")
        ),
        ok,
    ]

    result = external_files.download_content("https://example.com/file.txt", test_file)

    assert result == b"downloaded"
    assert mock_requests_get.call_count == 2
    assert mock_retry_sleep.call_args_list == [call(2)]


def test_has_remote_file_changed_retries_transient_error(
    mock_requests_head: MagicMock,
    mock_retry_sleep: MagicMock,
    setup_core: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A HEAD revalidation that fails transiently then returns 304 does not
    mark the cached copy stale, and the retry warning names the operation."""
    test_file = setup_core / "cached.txt"
    test_file.write_bytes(b"cached content")

    ok = MagicMock()
    ok.status_code = 304
    ok.headers = {}
    mock_requests_head.side_effect = [
        requests.exceptions.ConnectionError("reset by peer"),
        ok,
    ]

    changed = external_files.has_remote_file_changed(
        "https://example.com/file.txt", test_file
    )

    assert changed is False
    assert test_file not in external_files._run_data().stale_paths
    assert mock_requests_head.call_count == 2
    assert mock_retry_sleep.call_args_list == [call(2)]
    assert "Revalidation of" in caplog.text


def test_download_content_skip_external_update_uses_cache(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
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
    mock_has_remote_file_changed.assert_not_called()
    mock_requests_get.assert_not_called()
    # Deliberately unchecked is memoized for the run but never "fresh".
    assert not external_files.is_fresh_this_run(test_file)
    assert external_files.download_content(url, test_file) == cached_content
    mock_has_remote_file_changed.assert_not_called()


def test_download_content_skip_external_update_downloads_when_missing(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """Test download_content still downloads when file is missing, even with skip_external_update."""
    test_file = setup_core / "missing.txt"
    new_content = b"fresh content"

    mock_has_remote_file_changed.return_value = True
    mock_response = MagicMock()
    mock_response.content = new_content
    mock_response.headers = {}
    mock_response.raise_for_status = MagicMock()
    mock_requests_get.return_value = mock_response

    CORE.skip_external_update = True
    url = "https://example.com/file.txt"
    result = external_files.download_content(url, test_file)

    assert result == new_content
    assert test_file.read_bytes() == new_content


def test_download_content_many_empty_is_noop(
    mock_download_content: MagicMock, setup_core: Path
) -> None:
    """Empty input shouldn't spin up a thread pool or call download_content."""
    external_files.download_content_many([])
    mock_download_content.assert_not_called()


def test_download_content_many_single_item_avoids_pool(
    mock_download_content: MagicMock, setup_core: Path
) -> None:
    """A single item should be downloaded inline (no thread pool overhead)."""
    item = external_files.RemoteFile(
        "https://example.com/file.txt", setup_core / "f.txt"
    )
    external_files.download_content_many([item])
    mock_download_content.assert_called_once_with(
        item.url,
        item.path,
        external_files.NETWORK_TIMEOUT,
        allow_stale=True,
        return_content=False,
    )


def test_download_content_many_runs_in_parallel(
    mock_download_content: MagicMock, setup_core: Path
) -> None:
    """Multiple items should run concurrently — total wall time ≈ max latency."""
    import threading

    barrier = threading.Barrier(3)

    def slow_download(
        url: str,
        path: Path,
        *args: Any,
        **kwargs: Any,
    ) -> bytes:
        # If calls were serial this would deadlock (third caller never arrives
        # while the first is blocked at the barrier).
        barrier.wait(timeout=2.0)
        return b""

    mock_download_content.side_effect = slow_download
    items = [
        external_files.RemoteFile("https://example.com/a", setup_core / "a"),
        external_files.RemoteFile("https://example.com/b", setup_core / "b"),
        external_files.RemoteFile("https://example.com/c", setup_core / "c"),
    ]
    external_files.download_content_many(items, max_workers=4)
    assert mock_download_content.call_count == 3


def test_download_content_many_propagates_single_error(
    mock_download_content: MagicMock, setup_core: Path
) -> None:
    """A single failing worker should raise its `Invalid` directly, not wrap
    it in a `MultipleInvalid` that the caller would have to unpack.
    """

    def fake_download(
        url: str,
        path: Path,
        *args: Any,
        **kwargs: Any,
    ) -> bytes:
        if url.endswith("bad"):
            raise Invalid(f"could not download {url}")
        return b""

    mock_download_content.side_effect = fake_download
    items = [
        external_files.RemoteFile("https://example.com/ok", setup_core / "ok"),
        external_files.RemoteFile("https://example.com/bad", setup_core / "bad"),
    ]
    with pytest.raises(Invalid, match="could not download") as exc_info:
        external_files.download_content_many(items)
    assert not isinstance(exc_info.value, MultipleInvalid)


def test_download_content_many_aggregates_multiple_errors(
    mock_download_content: MagicMock, setup_core: Path
) -> None:
    """Every failing worker should be reported in a single MultipleInvalid so
    the user sees all broken URLs in one validation pass instead of fixing
    them one network round-trip at a time.
    """

    def fake_download(
        url: str,
        path: Path,
        *args: Any,
        **kwargs: Any,
    ) -> bytes:
        if url.endswith("ok"):
            return b""
        raise Invalid(f"could not download {url}")

    mock_download_content.side_effect = fake_download
    items = [
        external_files.RemoteFile("https://example.com/ok", setup_core / "ok"),
        external_files.RemoteFile("https://example.com/bad1", setup_core / "bad1"),
        external_files.RemoteFile("https://example.com/bad2", setup_core / "bad2"),
    ]
    with pytest.raises(MultipleInvalid) as exc_info:
        external_files.download_content_many(items)
    messages = {str(e) for e in exc_info.value.errors}
    assert messages == {
        "could not download https://example.com/bad1",
        "could not download https://example.com/bad2",
    }


def test_download_content_many_dedupes_by_path(
    mock_download_content: MagicMock, setup_core: Path
) -> None:
    """Two items pointing at the same cache path must collapse to one
    download -- otherwise concurrent writes race on the same file. Which
    URL wins doesn't matter (in practice duplicate paths only arise when
    the URL is duplicated), so we only assert the call count and path.
    """
    path = setup_core / "shared"
    items = [
        external_files.RemoteFile("https://example.com/a", path),
        external_files.RemoteFile("https://example.com/b", path),
        external_files.RemoteFile("https://example.com/a", path),
    ]
    external_files.download_content_many(items)
    assert mock_download_content.call_count == 1
    args, _ = mock_download_content.call_args
    assert args[1] == path


def test_download_content_many_clamps_invalid_max_workers(
    mock_download_content: MagicMock, setup_core: Path
) -> None:
    """`max_workers <= 0` must not raise from ThreadPoolExecutor; it should
    be clamped up to at least 1 worker.
    """
    items = [
        external_files.RemoteFile("https://example.com/a", setup_core / "a"),
        external_files.RemoteFile("https://example.com/b", setup_core / "b"),
    ]
    external_files.download_content_many(items, max_workers=0)
    assert mock_download_content.call_count == 2


def test_download_web_files_in_config_filters_and_dispatches(
    mock_download_content_many: MagicMock, setup_core: Path
) -> None:
    """Only `file.type == "web"` entries should be forwarded to
    download_content_many, and the unmodified config should be returned so
    the helper can sit in a `cv.All(...)` chain.
    """

    def path_for(file_dict: dict) -> Path:
        return setup_core / file_dict["url"].rsplit("/", 1)[-1]

    config = [
        {"file": {"type": "web", "url": "https://example.com/a"}},
        {"file": {"type": "local", "path": "/tmp/b"}},
        {"file": {"type": "web", "url": "https://example.com/c"}},
        {},  # no `file` key at all
    ]
    result = external_files.download_web_files_in_config(config, path_for)

    assert result is config
    mock_download_content_many.assert_called_once()
    assert list(mock_download_content_many.call_args[0][0]) == [
        external_files.RemoteFile("https://example.com/a", setup_core / "a"),
        external_files.RemoteFile("https://example.com/c", setup_core / "c"),
    ]


def test_download_web_files_in_config_no_web_entries(
    mock_download_content_many: MagicMock, setup_core: Path
) -> None:
    """A config with no web entries should still call through to
    download_content_many (which is itself a no-op for empty input) so the
    behavior stays consistent.
    """
    config = [{"file": {"type": "local", "path": "/tmp/a"}}]
    external_files.download_web_files_in_config(config, lambda _: setup_core / "x")
    mock_download_content_many.assert_called_once()
    assert list(mock_download_content_many.call_args[0][0]) == []


def test_download_content_saves_etag(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """Test download_content writes the ETag sidecar after a successful download."""
    test_file = setup_core / "fresh.txt"
    new_content = b"fresh content"

    mock_has_remote_file_changed.return_value = True
    mock_response = MagicMock()
    mock_response.content = new_content
    mock_response.headers = {external_files.ETAG: '"deadbeef"'}
    mock_response.raise_for_status = MagicMock()
    mock_requests_get.return_value = mock_response

    url = "https://example.com/file.txt"
    external_files.download_content(url, test_file)

    assert external_files._etag_sidecar_path(test_file).read_text() == '"deadbeef"'


def test_download_content_atomic_write_no_partial_on_failure(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    mock_write_file: MagicMock,
    setup_core: Path,
) -> None:
    """If `write_file` (the atomic-write helper) fails, the existing cache
    file must remain untouched and no temp files may be left behind. Patching
    `write_file` directly exercises the atomic-rename path -- a failure inside
    `write_file` is the only reason the rename wouldn't have happened.
    """
    from esphome.core import EsphomeError

    test_file = setup_core / "cached.txt"
    original_content = b"original content"
    test_file.write_bytes(original_content)

    mock_has_remote_file_changed.return_value = True
    mock_response = MagicMock()
    mock_response.content = b"new content"
    mock_response.headers = {}
    mock_response.raise_for_status = MagicMock()
    mock_requests_get.return_value = mock_response

    mock_write_file.side_effect = EsphomeError("disk full")

    with pytest.raises(EsphomeError, match="disk full"):
        external_files.download_content("https://example.com/file.txt", test_file)

    # Original file is untouched -- write_file aborted before its rename step.
    assert test_file.read_bytes() == original_content
    # write_file is responsible for cleaning its own temp files; nothing leaks
    # into the cache directory either way.
    leftover_tmps = list(setup_core.glob("tmp*"))
    assert leftover_tmps == []


def test_download_content_memoizes_fresh_path(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """A path downloaded once this run skips all network on later calls."""
    test_file = setup_core / "memo.txt"

    mock_has_remote_file_changed.return_value = True
    mock_response = MagicMock()
    mock_response.content = b"fresh content"
    mock_response.headers = {}
    mock_requests_get.return_value = mock_response

    url = "https://example.com/file.txt"
    assert external_files.download_content(url, test_file) == b"fresh content"
    assert external_files.download_content(url, test_file) == b"fresh content"

    mock_has_remote_file_changed.assert_called_once()
    mock_requests_get.assert_called_once()


def test_download_content_memo_revalidates_deleted_file(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """A memoized path whose file vanished is downloaded again."""
    test_file = setup_core / "memo.txt"

    mock_has_remote_file_changed.return_value = True
    mock_response = MagicMock()
    mock_response.content = b"fresh content"
    mock_response.headers = {}
    mock_requests_get.return_value = mock_response

    url = "https://example.com/file.txt"
    external_files.download_content(url, test_file)
    test_file.unlink()
    external_files.download_content(url, test_file)

    assert mock_requests_get.call_count == 2


def test_download_content_failure_fails_fast_on_retry(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """A failed download is remembered; a retry raises without network."""
    test_file = setup_core / "memo.txt"

    mock_has_remote_file_changed.return_value = True
    mock_requests_get.side_effect = requests.exceptions.RequestException("boom")

    url = "https://example.com/file.txt"
    with pytest.raises(Invalid, match="boom"):
        external_files.download_content(url, test_file)
    with pytest.raises(Invalid, match="boom"):
        external_files.download_content(url, test_file)

    mock_requests_get.assert_called_once()


def test_download_content_failed_path_revalidates_when_file_appears(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """A recorded failure is dropped once the file exists on disk."""
    test_file = setup_core / "memo.txt"

    mock_has_remote_file_changed.return_value = True
    mock_requests_get.side_effect = requests.exceptions.RequestException("boom")

    url = "https://example.com/file.txt"
    with pytest.raises(Invalid):
        external_files.download_content(url, test_file)

    # Another writer produced the file; the cached failure no longer applies
    # and the network error now falls back to the on-disk copy.
    test_file.write_bytes(b"appeared")
    assert external_files.download_content(url, test_file) == b"appeared"


def test_download_content_network_error_fallback_memoizes(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """Falling back to a cached file memoizes, so a flaky host is hit once."""
    test_file = setup_core / "memo.txt"
    test_file.write_bytes(b"cached content")

    mock_has_remote_file_changed.return_value = True
    mock_requests_get.side_effect = requests.exceptions.RequestException("boom")

    url = "https://example.com/file.txt"
    assert external_files.download_content(url, test_file) == b"cached content"
    assert external_files.download_content(url, test_file) == b"cached content"

    mock_requests_get.assert_called_once()


def test_download_content_not_changed_uses_cache(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """A 304 not-changed check serves the cached file without a GET."""
    test_file = setup_core / "cached.txt"
    test_file.write_bytes(b"cached content")
    mock_has_remote_file_changed.return_value = False

    url = "https://example.com/file.txt"
    assert external_files.download_content(url, test_file) == b"cached content"

    mock_requests_get.assert_not_called()


def test_head_failure_fallback_is_stale_not_fresh(
    mock_requests_head: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """A HEAD network failure serves the copy once and memoizes it as stale."""
    test_file = setup_core / "cached.txt"
    test_file.write_bytes(b"cached content")

    mock_requests_head.side_effect = requests.exceptions.RequestException("boom")

    url = "https://example.com/file.txt"
    assert external_files.download_content(url, test_file) == b"cached content"
    assert external_files.download_content(url, test_file) == b"cached content"

    mock_requests_head.assert_called_once()
    mock_requests_get.assert_not_called()


def test_allow_stale_false_rejects_unverified_copy(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """allow_stale=False raises instead of building from an unverified copy."""
    test_file = setup_core / "cached.txt"
    test_file.write_bytes(b"cached content")

    mock_has_remote_file_changed.return_value = True
    mock_requests_get.side_effect = requests.exceptions.RequestException("boom")

    url = "https://example.com/file.txt"
    with pytest.raises(Invalid, match="Could not download"):
        external_files.download_content(url, test_file, allow_stale=False)

    # A strict caller gets its own attempt at the network rather than
    # inheriting the stale memo's verdict.
    with pytest.raises(Invalid, match="Could not download"):
        external_files.download_content(url, test_file, allow_stale=False)
    assert mock_requests_get.call_count == 2

    # A caller that tolerates stale copies still gets the cached bytes.
    assert external_files.download_content(url, test_file) == b"cached content"


def test_allow_stale_false_rejects_head_failure_fallback(
    mock_requests_head: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """allow_stale=False also rejects a copy the HEAD could not confirm."""
    test_file = setup_core / "cached.txt"
    test_file.write_bytes(b"cached content")

    mock_requests_head.side_effect = requests.exceptions.RequestException("boom")

    url = "https://example.com/file.txt"
    with pytest.raises(Invalid, match="cannot be verified"):
        external_files.download_content(url, test_file, allow_stale=False)
    mock_requests_get.assert_not_called()


def test_download_content_many_forwards_per_file_allow_stale(
    mock_download_content: MagicMock, setup_core: Path
) -> None:
    """Each RemoteFile's own allow_stale reaches download_content."""
    files = [
        external_files.RemoteFile("https://example.com/a", setup_core / "a"),
        external_files.RemoteFile(
            "https://example.com/b", setup_core / "b", allow_stale=False
        ),
    ]
    external_files.download_content_many(files)
    forwarded = {
        call.args[1]: call.kwargs["allow_stale"]
        for call in mock_download_content.call_args_list
    }
    assert forwarded == {setup_core / "a": True, setup_core / "b": False}


def test_download_content_many_dedupe_keeps_strictest(
    mock_download_content: MagicMock, setup_core: Path
) -> None:
    """A strict duplicate wins over a permissive one for the same path."""
    path = setup_core / "fw.bin"
    files = [
        external_files.RemoteFile("https://example.com/fw", path, allow_stale=False),
        external_files.RemoteFile("https://example.com/fw", path),
    ]
    external_files.download_content_many(files)
    mock_download_content.assert_called_once()
    assert mock_download_content.call_args.kwargs["allow_stale"] is False


def test_successful_head_revalidation_clears_stale(
    mock_requests_head: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """A confirmed 304 supersedes an earlier failed revalidation."""
    test_file = setup_core / "cached.txt"
    test_file.write_bytes(b"cached content")

    ok_304 = MagicMock(status_code=304, headers={})
    mock_requests_head.side_effect = [
        requests.exceptions.RequestException("blip"),
        ok_304,
    ]

    url = "https://example.com/file.txt"
    assert external_files.download_content(url, test_file) == b"cached content"
    # The stale memo short-circuits tolerant callers; a strict caller
    # triggers a fresh HEAD, which now succeeds and clears the marker.
    assert (
        external_files.download_content(url, test_file, allow_stale=False)
        == b"cached content"
    )
    # Verified now: served from the fresh memo with no more network.
    assert external_files.download_content(url, test_file) == b"cached content"
    assert mock_requests_head.call_count == 2
    mock_requests_get.assert_not_called()


def test_failed_path_replay_names_the_other_url(
    mock_has_remote_file_changed: MagicMock,
    mock_requests_get: MagicMock,
    setup_core: Path,
) -> None:
    """A shared cache path replays the failure naming the original URL."""
    test_file = setup_core / "shared.bin"

    mock_has_remote_file_changed.return_value = True
    mock_requests_get.side_effect = requests.exceptions.RequestException("boom")

    with pytest.raises(Invalid, match="first-url"):
        external_files.download_content("https://example.com/first-url", test_file)
    with pytest.raises(Invalid, match="earlier download of.*first-url"):
        external_files.download_content("https://example.com/second-url", test_file)
    mock_requests_get.assert_called_once()
