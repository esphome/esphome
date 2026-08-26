from __future__ import annotations

from collections.abc import Callable, Iterable, Iterator
from concurrent.futures import ThreadPoolExecutor
import contextlib
from dataclasses import dataclass, field
from datetime import UTC, datetime
from functools import partial
import hashlib
import logging
import os
from pathlib import Path
import time

import esphome.config_validation as cv
from esphome.const import CONF_FILE, CONF_TYPE, CONF_URL, __version__
from esphome.core import CORE, EsphomeError, TimePeriodSeconds
from esphome.helpers import write_file
from esphome.net_retry import fetch_with_retry, http_request
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@landonr"]

DOMAIN = "external_files"

NETWORK_TIMEOUT = 30


@dataclass(frozen=True, slots=True)
class RemoteFile:
    """A remote file to prefetch, yielded in stages by ``PREFETCH_FILES``
    hooks. A dataclass rather than a tuple so fields can be added later."""

    url: str
    path: Path
    # False when nothing downstream can verify the bytes; a copy that
    # cannot be revalidated is then an error, not a silent fallback.
    allow_stale: bool = True


@dataclass(frozen=True, slots=True)
class FailedDownload:
    """What went wrong for a cache path this run, kept for fast replay."""

    url: str
    message: str
    cause: BaseException


@dataclass
class ExternalFilesRunData:
    """Per-run download state, cleared by ``CORE.reset()`` between runs."""

    # Verified fresh this run; later touches skip even the conditional HEAD.
    fresh_paths: set[Path] = field(default_factory=set)
    # Served from disk without revalidation; strict callers reject these.
    stale_paths: set[Path] = field(default_factory=set)
    # Served under skip_external_update, deliberately unchecked; skips the
    # network like fresh_paths but never counts as verified.
    unchecked_paths: set[Path] = field(default_factory=set)
    # Failed with no usable copy; later touches replay the error fast.
    failed_paths: dict[Path, FailedDownload] = field(default_factory=dict)


def _run_data() -> ExternalFilesRunData:
    if (data := CORE.data.get(DOMAIN)) is not None:
        return data
    # setdefault: first touch may race on download_content_many's workers.
    return CORE.data.setdefault(DOMAIN, ExternalFilesRunData())


IF_MODIFIED_SINCE = "If-Modified-Since"
IF_NONE_MATCH = "If-None-Match"
ETAG = "ETag"
CACHE_CONTROL = "Cache-Control"
CACHE_CONTROL_MAX_AGE = "max-age="
CONTENT_DISPOSITION = "content-disposition"
TEMP_DIR = "temp"


def _etag_sidecar_path(local_file_path: Path) -> Path:
    return local_file_path.parent / f".{local_file_path.name}.etag"


def _mtime_seconds(path: Path) -> int:
    """Return `path`'s mtime as integer seconds.

    Whole seconds is the common-denominator resolution across all
    filesystems we run on (FAT/exFAT 2s, NTFS 100ns, APFS/ext4 ns), so
    comparisons survive setting+reading round-trips that would lose
    sub-second precision on lower-resolution filesystems.
    """
    return int(path.stat().st_mtime)


def _read_etag(local_file_path: Path) -> str | None:
    """Return the cached ETag if its sidecar's mtime still matches the cache
    file's. A mismatch means the cache file was modified out-of-band, so the
    ETag no longer describes its contents -- delete the stale sidecar and
    return None.
    """
    etag_path = _etag_sidecar_path(local_file_path)
    try:
        if _mtime_seconds(etag_path) != _mtime_seconds(local_file_path):
            _LOGGER.debug(
                "ETag sidecar mtime mismatch at %s; treating as stale",
                local_file_path,
            )
            etag_path.unlink()
            return None
        return etag_path.read_text().strip() or None
    except OSError:
        return None


def _write_etag(local_file_path: Path, etag: str | None) -> None:
    etag_path = _etag_sidecar_path(local_file_path)
    if not etag:
        # ETag persistence is best-effort; matches `_read_etag`'s tolerance.
        with contextlib.suppress(OSError):
            etag_path.unlink()
        return
    try:
        write_file(etag_path, etag)
    except EsphomeError as e:
        _LOGGER.debug("Could not save ETag for %s: %s", local_file_path, e)
        return
    # Pin the sidecar's mtime to the cache file's mtime. _read_etag relies on
    # this match to detect out-of-band edits to the cache file.
    try:
        file_mtime = _mtime_seconds(local_file_path)
        os.utime(etag_path, (file_mtime, file_mtime))
    except OSError as e:
        _LOGGER.debug(
            "Could not sync ETag sidecar mtime for %s: %s", local_file_path, e
        )


def has_remote_file_changed(
    url: str, local_file_path: Path, timeout: int = NETWORK_TIMEOUT
) -> bool:
    # Deferred so configs with no remote files skip the heavy import.
    import requests

    if local_file_path.exists():
        _LOGGER.debug("has_remote_file_changed: File exists at %s", local_file_path)
        try:
            local_modification_time = local_file_path.stat().st_mtime
            local_modification_time_str = datetime.fromtimestamp(
                local_modification_time, tz=UTC
            ).strftime("%a, %d %b %Y %H:%M:%S GMT")

            headers = {
                IF_MODIFIED_SINCE: local_modification_time_str,
                CACHE_CONTROL: CACHE_CONTROL_MAX_AGE + "3600",
            }
            if etag := _read_etag(local_file_path):
                headers[IF_NONE_MATCH] = etag
            # Retried so allow_stale=False consumers don't hard-fail on a
            # healed flake. Only connection-level failures retry: HEAD
            # never raises on HTTP status (servers rejecting HEAD with
            # 405/501 must fall through to the GET), so 5xx is handled by
            # the GET's own retry.
            response = fetch_with_retry(
                url,
                partial(http_request, "HEAD", url, headers=headers, timeout=timeout),
                what="Revalidation",
            )

            _LOGGER.debug(
                "has_remote_file_changed: File %s, Local modified %s, ETag %s, response code %d",
                local_file_path,
                local_modification_time_str,
                etag or "<none>",
                response.status_code,
            )

            if response.status_code == 304:
                _LOGGER.debug(
                    "has_remote_file_changed: File not modified since %s",
                    local_modification_time_str,
                )
                if (new_etag := response.headers.get(ETAG)) and new_etag != etag:
                    _write_etag(local_file_path, new_etag)
                # A confirmed 304 supersedes any earlier failed
                # revalidation of this file.
                _run_data().stale_paths.discard(local_file_path)
                return False
            _LOGGER.debug("has_remote_file_changed: File modified")
            return True
        except requests.exceptions.RequestException as e:
            _LOGGER.warning(
                "Could not check if %s has changed due to network error (%s), using cached file",
                url,
                e,
            )
            # The copy is a fallback, not a verified 304; record that so
            # callers that must not use unverified bytes can reject it.
            _run_data().stale_paths.add(local_file_path)
            return False

    _LOGGER.debug("has_remote_file_changed: File doesn't exists at %s", local_file_path)
    return True


def is_file_recent(file_path: Path, refresh: TimePeriodSeconds) -> bool:
    if file_path.exists():
        # st_mtime, not st_ctime: ctime is inode-change time on POSIX
        # (bumped by chmod/chown/rename) so a metadata touch would make
        # the file look fresh.
        modification_time = file_path.stat().st_mtime
        return time.time() - modification_time <= refresh.total_seconds
    return False


def compute_local_file_dir(domain: str) -> Path:
    base_directory = Path(CORE.data_dir) / domain
    base_directory.mkdir(parents=True, exist_ok=True)

    return base_directory


def url_cache_key(url: str) -> str:
    """Short stable cache key for a URL."""
    return hashlib.sha256(url.encode()).hexdigest()[:8]


def compute_local_file_path(domain: str, url: str) -> Path:
    """Cache path for a URL-keyed download under the domain's cache dir.

    Pure (no mkdir); parent directories are created at write time.
    """
    return Path(CORE.data_dir) / domain / url_cache_key(url)


def is_fresh_this_run(path: Path) -> bool:
    """Whether `path` was verified or downloaded during this run."""
    return path in _run_data().fresh_paths


def download_content(
    url: str,
    path: Path,
    timeout: int = NETWORK_TIMEOUT,
    allow_stale: bool = True,
    return_content: bool = True,
) -> bytes:
    """Download `url` into `path` and return the bytes, using the cache.

    On network failure an on-disk copy is served with a warning, unless
    ``allow_stale=False``. ``CORE.skip_external_update`` always serves the
    copy. ``return_content=False`` skips the disk read on cache hits.
    """

    # Deferred so configs with no remote files skip the heavy import.
    import requests

    def _cached() -> bytes:
        return path.read_bytes() if return_content else b""

    # Memoized paths skip the network entirely; concurrent access is safe
    # because download_content_many dedupes by path before fanning out.
    run_data = _run_data()
    fresh_paths = run_data.fresh_paths
    if (path in fresh_paths or path in run_data.unchecked_paths) and path.exists():
        return _cached()
    if allow_stale and path in run_data.stale_paths and path.exists():
        # Strict callers fall through to try the network themselves.
        _LOGGER.info("Using cached copy of %s that could not be revalidated", url)
        return _cached()
    if (failure := run_data.failed_paths.get(path)) is not None:
        if not path.exists():
            if failure.url == url:
                raise cv.Invalid(failure.message) from failure.cause
            raise cv.Invalid(
                f"Could not download from {url}: an earlier download of "
                f"{failure.url} to the same cache file failed: {failure.cause}"
            ) from failure.cause
        # The file appeared since the failure; revalidate normally.
        del run_data.failed_paths[path]
    if CORE.skip_external_update and path.exists():
        _LOGGER.debug("Skipping update for %s (refresh disabled)", url)
        run_data.unchecked_paths.add(path)
        return _cached()
    if not has_remote_file_changed(url, path, timeout):
        if path in run_data.stale_paths:
            # The HEAD fell back to the copy without confirming it.
            if not allow_stale:
                raise cv.Invalid(
                    f"Could not check {url} for updates due to a network error "
                    f"and the cached copy cannot be verified"
                )
            return _cached()
        _LOGGER.debug("Remote file has not changed %s", url)
        fresh_paths.add(path)
        return _cached()

    _LOGGER.info("Downloading %s", url)
    _LOGGER.debug("Saving to %s", path)

    def _fetch() -> tuple[requests.Response, bytes]:
        req = http_request(
            "GET",
            url,
            timeout=timeout,
            headers={"User-agent": f"ESPHome/{__version__} (https://esphome.io)"},
        )
        req.raise_for_status()
        # `.content` reads the body lazily; chunked-decode, gzip-decode,
        # and mid-stream connection errors all surface here as
        # RequestException subclasses, so this needs the same fall-back
        # treatment as the request itself.
        return req, req.content

    try:
        req, data = fetch_with_retry(url, _fetch)
    except requests.exceptions.RequestException as e:
        if path.exists():
            # Memoized so a flaky host warns once per run, not per consumer.
            run_data.stale_paths.add(path)
            if not allow_stale:
                raise cv.Invalid(f"Could not download from {url}: {e}") from e
            _LOGGER.warning(
                "Could not download from %s due to network error (%s), using cached file",
                url,
                e,
            )
            return _cached()
        message = f"Could not download from {url}: {e}"
        run_data.failed_paths[path] = FailedDownload(url, message, e)
        raise cv.Invalid(message) from e

    write_file(path, data)
    _write_etag(path, req.headers.get(ETAG))
    fresh_paths.add(path)
    run_data.stale_paths.discard(path)
    return data


# Cap concurrent connections so a config with hundreds of remote files doesn't
# open hundreds of sockets at once. 8 matches the requests connection-pool
# default and the per-host connection limit browsers use, which keeps us
# polite to the upstream host while still cutting wall time roughly 8x for
# typical configs (a couple dozen files).
DEFAULT_DOWNLOAD_WORKERS = 8


def download_content_many(
    items: Iterable[RemoteFile],
    timeout: int = NETWORK_TIMEOUT,
    max_workers: int = DEFAULT_DOWNLOAD_WORKERS,
    description: str = "remote file(s)",
) -> None:
    """Run `download_content` for each `RemoteFile` concurrently.

    `description` names the files in the progress log line. All workers run
    to completion; every `cv.Invalid` raised is surfaced together as
    `cv.MultipleInvalid`. Items dedupe by `path` (avoiding write races on
    the same cache file); the last URL wins and a strict
    `allow_stale=False` from any duplicate is kept.
    """
    seen: dict[Path, RemoteFile] = {}
    for file in items:
        if (prior := seen.get(file.path)) is not None and not prior.allow_stale:
            file = RemoteFile(file.url, file.path, allow_stale=False)
        seen[file.path] = file
    unique = list(seen.values())
    if not unique:
        return
    _LOGGER.info("Checking %d %s for updates", len(unique), description)

    def _download_one(file: RemoteFile) -> None:
        download_content(
            file.url,
            file.path,
            timeout,
            allow_stale=file.allow_stale,
            return_content=False,
        )

    if len(unique) == 1:
        _download_one(unique[0])
        return

    workers = max(1, min(max_workers, len(unique)))
    errors: list[cv.Invalid] = []
    with ThreadPoolExecutor(max_workers=workers) as ex:
        futures = [ex.submit(_download_one, file) for file in unique]
        for future in futures:
            try:
                future.result()
            except cv.Invalid as e:
                errors.append(e)
    if not errors:
        return
    if len(errors) == 1:
        raise errors[0]
    raise cv.MultipleInvalid(errors)


def single_stage_prefetch(
    extract: Callable[[ConfigType], RemoteFile | None],
) -> Callable[[list[ConfigType]], Iterator[list[RemoteFile]]]:
    """Build a one-batch ``PREFETCH_FILES`` hook from a per-entry extractor.

    Covers the common case of one remote file per raw config entry;
    components with staged downloads write their own generator.
    """

    def prefetch_files(entries: list[ConfigType]) -> Iterator[list[RemoteFile]]:
        yield [ref for entry in entries if (ref := extract(entry)) is not None]

    return prefetch_files


# Each component that uses external_files defines its own local
# `TYPE_WEB = "web"`; the string is repeated here rather than imported
# because there is no canonical `TYPE_WEB` in `esphome.const` to share.
WEB_TYPE = "web"


def download_web_files_in_config(
    config: list[ConfigType],
    path_for: Callable[[ConfigType], Path],
) -> list[ConfigType]:
    """Voluptuous-friendly validator that downloads any web-sourced files in
    `config` in parallel.

    Each entry is expected to contain a `file` key whose value is a dict
    that may be `{type: "web", url: ...}`; `path_for(file_dict)` returns
    the cache path for that file. Returns `config` unchanged so it can be
    slotted directly into a `cv.All(...)` chain.
    """
    download_content_many(
        RemoteFile(conf_file[CONF_URL], path_for(conf_file))
        for entry in config
        if (conf_file := entry.get(CONF_FILE, {})).get(CONF_TYPE) == WEB_TYPE
    )
    return config
