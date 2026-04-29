from __future__ import annotations

import contextlib
from datetime import UTC, datetime
import logging
import os
from pathlib import Path

import requests

import esphome.config_validation as cv
from esphome.const import __version__
from esphome.core import CORE, EsphomeError, TimePeriodSeconds
from esphome.helpers import write_file

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
            if etag := _read_etag(local_file_path):
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
