from __future__ import annotations

from datetime import datetime
import logging
import os
from pathlib import Path

import requests

import esphome.config_validation as cv
from esphome.const import __version__
from esphome.core import CORE, TimePeriodSeconds

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@landonr"]

NETWORK_TIMEOUT = 30

IF_MODIFIED_SINCE = "If-Modified-Since"
CACHE_CONTROL = "Cache-Control"
CACHE_CONTROL_MAX_AGE = "max-age="
CONTENT_DISPOSITION = "content-disposition"
TEMP_DIR = "temp"


def has_remote_file_changed(url, local_file_path):
    if os.path.exists(local_file_path):
        _LOGGER.debug("has_remote_file_changed: File exists at %s", local_file_path)
        try:
            local_modification_time = os.path.getmtime(local_file_path)
            local_modification_time_str = datetime.utcfromtimestamp(
                local_modification_time
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
            raise cv.Invalid(
                f"Could not check if {url} has changed, please check if file exists "
                f"({e})"
            )

    _LOGGER.debug("has_remote_file_changed: File doesn't exists at %s", local_file_path)
    return True


def is_file_recent(file_path: str, refresh: TimePeriodSeconds) -> bool:
    if os.path.exists(file_path):
        creation_time = os.path.getctime(file_path)
        current_time = datetime.now().timestamp()
        return current_time - creation_time <= refresh.total_seconds
    return False


def compute_local_file_dir(domain: str) -> Path:
    base_directory = Path(CORE.data_dir) / domain
    base_directory.mkdir(parents=True, exist_ok=True)

    return base_directory


def download_content(url: str, path: Path, timeout=NETWORK_TIMEOUT) -> bytes:
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
        raise cv.Invalid(f"Could not download from {url}: {e}")

    path.parent.mkdir(parents=True, exist_ok=True)
    data = req.content
    path.write_bytes(data)
    return data
