import logging
from pathlib import Path
import os
from urllib.parse import urlparse, unquote
from datetime import datetime
import re
import requests
import esphome.config_validation as cv
from esphome.core import CORE, TimePeriodSeconds
from esphome.const import __version__

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
            response = requests.get(url, headers=headers, timeout=NETWORK_TIMEOUT)

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


def compute_local_file_dir(name: str, domain: str) -> Path:
    base_directory = Path(CORE.data_dir) / domain
    if not os.path.exists(base_directory):
        os.makedirs(base_directory)

    file_path = os.path.join(base_directory, name)

    return Path(file_path)


def get_file_info_from_content_disposition(r):
    FILE_NAME_REGEX = (
        r'filename\*?=UTF-8\'\'([^;\n"]+)|filename=["\']([^"\']+)|filename=([^;\n]+)'
    )
    cd = r.headers.get(CONTENT_DISPOSITION)
    if not cd:
        return None
    _LOGGER.debug("content disposition=%s", cd)
    match = re.search(FILE_NAME_REGEX, cd)
    if match:
        file_name = match.group(1) or match.group(2) or match.group(3)
        _LOGGER.debug("file_name=%s", file_name)
        file_base_name, file_extension = os.path.splitext(file_name)
        return file_base_name, file_extension
    return None


def parse_file_info_from_url(url):
    parsed_url = urlparse(url)
    if parsed_url.fragment:
        # handle urls like https://en.wikipedia.org/wiki/Standard_test_image#/media/File:SIPI_Jelly_Beans_4.1.07.tiff
        path = unquote(parsed_url.fragment)
        file_name = os.path.basename(path)
        if file_name.startswith("File:"):
            file_name = file_name[len("File:") :]
    else:
        path = unquote(parsed_url.path)
        file_name = os.path.basename(path)
    _LOGGER.debug("parse_file_info_from_url: file_name=%s path=%s", file_name, path)
    file_base_name, file_extension = os.path.splitext(file_name)
    return file_base_name, file_extension


def get_file_info_from_url(url):
    _LOGGER.debug("get file info url %s", url)
    r = requests.get(
        url,
        allow_redirects=True,
        timeout=NETWORK_TIMEOUT,
        headers={"User-agent": f"ESPHome/{__version__} (https://esphome.io)"},
    )
    if r.status_code != 200:
        raise cv.Invalid(
            f"Could not check {url} info, check if file exists ({r.status_code}"
        )
    if CONTENT_DISPOSITION in r.headers:
        file_base_name, file_extension = get_file_info_from_content_disposition(r)
        _LOGGER.debug(
            "file at url has content disposition %s %s",
            file_base_name,
            file_extension,
        )
        return file_base_name, file_extension, None
    file_base_name, file_extension = parse_file_info_from_url(url)
    file_name = file_base_name + file_extension
    _LOGGER.debug(
        "get_file_info_from_url: file at url has no content disposition file_base_name=%s file_extension=%s",
        file_base_name,
        file_extension,
    )
    temp_path = compute_local_file_dir(file_name, TEMP_DIR)
    _LOGGER.debug("get_file_info_from_url: downloading to temp_path=%s", temp_path)
    with open(temp_path, "wb") as f:
        f.write(r.content)
    return file_base_name, file_extension, temp_path
