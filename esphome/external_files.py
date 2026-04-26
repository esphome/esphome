from __future__ import annotations

from collections.abc import Callable, Iterable
from concurrent.futures import ThreadPoolExecutor
import contextlib
from datetime import UTC, datetime
import logging
import os
from pathlib import Path

import requests

import esphome.config_validation as cv
from esphome.const import CONF_FILE, CONF_TYPE, CONF_URL, __version__
from esphome.core import CORE, EsphomeError, TimePeriodSeconds
from esphome.helpers import write_file
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@landonr"]

NETWORK_TIMEOUT = 30

IF_MODIFIED_SINCE = "If-Modified-Since"
IF_NONE_MATCH = "If-None-Match"
ETAG = "ETag"
CACHE_CONTROL = "Cache-Control"
CACHE_CONTROL_MAX_AGE = "max-age="
CONTENT_DISPOSITION = "content-disposition"
TEMP_DIR = "temp"


def _etag_sidecar_path(local_file_path: Path) -> Path:
    return local_file_path.parent / f".{local_file_path.name}.etag"


def _read_etag(local_file_path: Path) -> str | None:
    """Return the cached ETag if it still describes the current cached file.

    The sidecar's mtime is kept in sync with the cache file's mtime by
    `_write_etag`. If they disagree, the cache file was modified out-of-band
    (manual edit, restored from backup, replaced by another tool) and the
    ETag no longer corresponds to its contents, so treat the sidecar as
    stale and delete it.
    """
    etag_path = _etag_sidecar_path(local_file_path)
    try:
        etag_mtime_ns = etag_path.stat().st_mtime_ns
        file_mtime_ns = local_file_path.stat().st_mtime_ns
    except OSError:
        return None
    if etag_mtime_ns != file_mtime_ns:
        _LOGGER.debug(
            "ETag sidecar mtime (%d) does not match cached file mtime (%d) at %s; "
            "treating ETag as stale",
            etag_mtime_ns,
            file_mtime_ns,
            local_file_path,
        )
        with contextlib.suppress(OSError):
            etag_path.unlink()
        return None
    try:
        etag = etag_path.read_text().strip()
    except OSError:
        return None
    return etag or None


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
        file_mtime_ns = local_file_path.stat().st_mtime_ns
        os.utime(etag_path, ns=(file_mtime_ns, file_mtime_ns))
    except OSError as e:
        _LOGGER.debug(
            "Could not sync ETag sidecar mtime for %s: %s", local_file_path, e
        )


def has_remote_file_changed(url: str, local_file_path: Path) -> bool:
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
            etag = _read_etag(local_file_path)
            if etag:
                headers[IF_NONE_MATCH] = etag
            response = requests.head(
                url, headers=headers, timeout=NETWORK_TIMEOUT, allow_redirects=True
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
                new_etag = response.headers.get(ETAG)
                if new_etag and new_etag != etag:
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
            return False

    _LOGGER.debug("has_remote_file_changed: File doesn't exists at %s", local_file_path)
    return True


def is_file_recent(file_path: Path, refresh: TimePeriodSeconds) -> bool:
    if file_path.exists():
        creation_time = file_path.stat().st_ctime
        current_time = datetime.now().timestamp()
        return current_time - creation_time <= refresh.total_seconds
    return False


def compute_local_file_dir(domain: str) -> Path:
    base_directory = Path(CORE.data_dir) / domain
    base_directory.mkdir(parents=True, exist_ok=True)

    return base_directory


def download_content(url: str, path: Path, timeout: int = NETWORK_TIMEOUT) -> bytes:
    if CORE.skip_external_update and path.exists():
        _LOGGER.debug("Skipping update for %s (refresh disabled)", url)
        return path.read_bytes()
    if not has_remote_file_changed(url, path):
        _LOGGER.debug("Remote file has not changed %s", url)
        return path.read_bytes()

    _LOGGER.debug(
        "Remote file has changed, downloading from %s to %s",
        url,
        path,
    )

    try:
        req = requests.get(
            url,
            timeout=timeout,
            headers={"User-agent": f"ESPHome/{__version__} (https://esphome.io)"},
        )
        req.raise_for_status()
    except requests.exceptions.RequestException as e:
        if path.exists():
            _LOGGER.warning(
                "Could not download from %s due to network error (%s), using cached file",
                url,
                e,
            )
            return path.read_bytes()
        raise cv.Invalid(f"Could not download from {url}: {e}") from e

    data = req.content
    write_file(path, data)
    _write_etag(path, req.headers.get(ETAG))
    return data


# Cap concurrent connections so a config with hundreds of remote files doesn't
# open hundreds of sockets at once. 8 matches the requests connection-pool
# default and the per-host connection limit browsers use, which keeps us
# polite to the upstream host while still cutting wall time roughly 8x for
# typical configs (a couple dozen files).
DEFAULT_DOWNLOAD_WORKERS = 8


def download_content_many(
    items: Iterable[tuple[str, Path]],
    timeout: int = NETWORK_TIMEOUT,
    max_workers: int = DEFAULT_DOWNLOAD_WORKERS,
) -> None:
    """Run `download_content` for each (url, path) pair concurrently.

    Wall time drops from `sum(latency)` to roughly `max(latency)` for cached
    files where the HEAD round-trip dominates. The first exception raised by
    any worker is propagated; remaining workers complete before this returns.

    Items are de-duplicated by `path` -- two callers asking for the same
    cache file (e.g. the same URL referenced twice in a config) would
    otherwise race on `download_content`'s non-atomic write.
    """
    seen: dict[Path, str] = {}
    for url, path in items:
        if path in seen:
            continue
        seen[path] = url
    if not seen:
        return
    if len(seen) == 1:
        path, url = next(iter(seen.items()))
        download_content(url, path, timeout)
        return
    workers = max(1, min(max_workers, len(seen)))
    with ThreadPoolExecutor(max_workers=workers) as ex:
        # list() forces iteration so exceptions surface here, not silently.
        list(
            ex.map(
                lambda item: download_content(item[1], item[0], timeout),
                seen.items(),
            )
        )


# String constant rather than `from .const import TYPE_WEB` because each
# component defines its own `TYPE_WEB = "web"` literal in its module scope.
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
        (conf_file[CONF_URL], path_for(conf_file))
        for entry in config
        if (conf_file := entry.get(CONF_FILE, {})).get(CONF_TYPE) == WEB_TYPE
    )
    return config
