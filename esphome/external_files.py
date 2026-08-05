from __future__ import annotations

from collections.abc import Callable, Iterable
from concurrent.futures import ThreadPoolExecutor
import contextlib
from dataclasses import dataclass, field
from datetime import UTC, datetime
import hashlib
import logging
import os
from pathlib import Path
import time

import requests

import esphome.config_validation as cv
from esphome.const import CONF_FILE, CONF_TYPE, CONF_URL, __version__
from esphome.core import CORE, EsphomeError, TimePeriodSeconds
from esphome.happy_eyeballs import ensure_happy_eyeballs
from esphome.helpers import write_file
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@landonr"]

DOMAIN = "external_files"

NETWORK_TIMEOUT = 30


@dataclass(frozen=True, slots=True)
class RemoteFile:
    """A remote file a component wants prefetched into the download cache.

    Yielded (in per-stage batches) by component ``PREFETCH_FILES`` hooks. A
    dataclass rather than a tuple so new fields can be added without
    breaking existing hooks.
    """

    url: str
    path: Path


@dataclass
class ExternalFilesRunData:
    """Per-run download state, cleared by ``CORE.reset()`` between runs."""

    # Paths whose freshness was verified (304 or a fresh download) this
    # run; later touches skip even the conditional HEAD.
    fresh_paths: set[Path] = field(default_factory=set)
    # Paths served from an on-disk copy that could NOT be revalidated this
    # run because the network failed; usable as a fallback, but callers
    # that must not build from unverified bytes reject these.
    stale_paths: set[Path] = field(default_factory=set)
    # Paths whose download already failed this run with no usable copy,
    # mapped to the error message; retrying within one run would only
    # repeat the timeout, so later touches fail fast with the recorded
    # error instead.
    failed_paths: dict[Path, str] = field(default_factory=dict)


def _run_data() -> ExternalFilesRunData:
    # setdefault, not check-then-set: first touch can happen concurrently on
    # download_content_many's worker threads, and a lost instance would
    # silently drop its memo entries.
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
    ensure_happy_eyeballs()
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
            response = requests.head(
                url, headers=headers, timeout=timeout, allow_redirects=True
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
    """Cache path for a URL-keyed download under the domain's cache dir."""
    return compute_local_file_dir(domain) / url_cache_key(url)


def download_content(
    url: str, path: Path, timeout: int = NETWORK_TIMEOUT, allow_stale: bool = True
) -> bytes:
    """Download `url` into `path` and return the bytes, using the cache.

    ``allow_stale`` controls what happens when the network fails but an
    on-disk copy exists: by default the copy is served with a warning;
    callers whose bytes cannot be verified downstream (e.g. firmware
    without a checksum) pass False to fail instead of building from a
    possibly outdated copy.
    """
    # A path verified once this run is served from disk with no further
    # network, not even the conditional HEAD; a path that already failed
    # this run fails fast with the recorded error. Concurrent access is
    # safe because download_content_many dedupes by path before fanning
    # out.
    run_data = _run_data()
    fresh_paths = run_data.fresh_paths
    if path in fresh_paths and path.exists():
        return path.read_bytes()
    if path in run_data.stale_paths and path.exists():
        if not allow_stale:
            raise cv.Invalid(
                f"Could not check {url} for updates due to a network error and "
                f"the cached copy cannot be verified"
            )
        _LOGGER.debug("Using cached copy of %s that could not be revalidated", url)
        return path.read_bytes()
    if (cached_error := run_data.failed_paths.get(path)) is not None:
        if not path.exists():
            raise cv.Invalid(cached_error)
        # Something produced the file since the failure; fall through and
        # revalidate it normally.
        del run_data.failed_paths[path]
    ensure_happy_eyeballs()
    if CORE.skip_external_update and path.exists():
        _LOGGER.debug("Skipping update for %s (refresh disabled)", url)
        fresh_paths.add(path)
        return path.read_bytes()
    if not has_remote_file_changed(url, path, timeout):
        if path in run_data.stale_paths:
            # The HEAD failed and fell back to the copy rather than
            # confirming it; route through the stale policy.
            if not allow_stale:
                raise cv.Invalid(
                    f"Could not check {url} for updates due to a network error "
                    f"and the cached copy cannot be verified"
                )
            return path.read_bytes()
        _LOGGER.debug("Remote file has not changed %s", url)
        fresh_paths.add(path)
        return path.read_bytes()

    _LOGGER.info("Downloading %s", url)
    _LOGGER.debug("Saving to %s", path)

    try:
        req = requests.get(
            url,
            timeout=timeout,
            headers={"User-agent": f"ESPHome/{__version__} (https://esphome.io)"},
        )
        req.raise_for_status()
        # `.content` reads the body lazily; chunked-decode, gzip-decode,
        # and mid-stream connection errors all surface here as
        # RequestException subclasses, so this needs the same fall-back
        # treatment as the request itself.
        data = req.content
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
            return path.read_bytes()
        message = f"Could not download from {url}: {e}"
        run_data.failed_paths[path] = message
        raise cv.Invalid(message) from e

    write_file(path, data)
    _write_etag(path, req.headers.get(ETAG))
    fresh_paths.add(path)
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
    allow_stale: bool = True,
) -> None:
    """Run `download_content` for each `RemoteFile` concurrently.

    `description` names the kind of files in the progress log line, e.g.
    "wake word manifest(s)". `allow_stale` is forwarded to
    `download_content` for every file.

    Wall time drops from `sum(latency)` to roughly `max(latency)` for cached
    files where the HEAD round-trip dominates. All workers run to
    completion before this returns; every `cv.Invalid` raised by a worker
    is collected and surfaced together as `cv.MultipleInvalid` so the user
    sees every broken file in a single validation pass instead of fixing
    them one round-trip at a time.

    Items are de-duplicated by `path` -- two callers asking for the same
    cache file (e.g. the same URL referenced twice in a config) would
    otherwise race on `download_content`'s non-atomic write. When the
    same `path` appears more than once, the last URL wins (standard dict
    comprehension semantics); in practice duplicate paths only arise when
    the URL is duplicated, so the choice doesn't matter.
    """
    seen: dict[Path, str] = {file.path: file.url for file in items}
    if not seen:
        return
    ensure_happy_eyeballs()
    _LOGGER.info("Checking %d %s for updates", len(seen), description)
    if len(seen) == 1:
        path, url = next(iter(seen.items()))
        download_content(url, path, timeout, allow_stale=allow_stale)
        return

    def _download_one(path_url: tuple[Path, str]) -> None:
        # `seen` stores entries as (path, url) so the dict can dedupe by
        # path; flip them back to download_content's (url, path) order.
        path, url = path_url
        download_content(url, path, timeout, allow_stale=allow_stale)

    workers = max(1, min(max_workers, len(seen)))
    errors: list[cv.Invalid] = []
    with ThreadPoolExecutor(max_workers=workers) as ex:
        futures = [ex.submit(_download_one, item) for item in seen.items()]
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
