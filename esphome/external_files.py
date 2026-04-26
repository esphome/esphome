from __future__ import annotations

from collections.abc import Callable, Iterable
from concurrent.futures import ThreadPoolExecutor
from datetime import UTC, datetime
import logging
from pathlib import Path

import requests

import esphome.config_validation as cv
from esphome.const import CONF_FILE, CONF_TYPE, CONF_URL, __version__
from esphome.core import CORE, TimePeriodSeconds
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@landonr"]

NETWORK_TIMEOUT = 30

IF_MODIFIED_SINCE = "If-Modified-Since"
CACHE_CONTROL = "Cache-Control"
CACHE_CONTROL_MAX_AGE = "max-age="
CONTENT_DISPOSITION = "content-disposition"
TEMP_DIR = "temp"


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
            response = requests.head(
                url, headers=headers, timeout=NETWORK_TIMEOUT, allow_redirects=True
            )

            _LOGGER.debug(
                "has_remote_file_changed: File %s, Local modified %s, response code %d",
                local_file_path,
                local_modification_time_str,
                response.status_code,
            )

            if response.status_code == 304:
                _LOGGER.debug(
                    "has_remote_file_changed: File not modified since %s",
                    local_modification_time_str,
                )
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

    path.parent.mkdir(parents=True, exist_ok=True)
    data = req.content
    path.write_bytes(data)
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
    files where the HEAD round-trip dominates. Worker exceptions propagate
    when iteration reaches the corresponding input item (`ex.map` yields
    results in input order), and remaining workers complete before this
    returns.

    Items are de-duplicated by `path` -- two callers asking for the same
    cache file (e.g. the same URL referenced twice in a config) would
    otherwise race on `download_content`'s non-atomic write. When the
    same `path` appears more than once, the last URL wins (standard dict
    comprehension semantics); in practice duplicate paths only arise when
    the URL is duplicated, so the choice doesn't matter.
    """
    seen: dict[Path, str] = {path: url for url, path in items}
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
        (conf_file[CONF_URL], path_for(conf_file))
        for entry in config
        if (conf_file := entry.get(CONF_FILE, {})).get(CONF_TYPE) == WEB_TYPE
    )
    return config
