import logging
from pathlib import Path
import os
from urllib.parse import urlparse, unquote
from datetime import datetime
import re
import requests
import esphome.config_validation as cv
from esphome.core import CORE, TimePeriodSeconds

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@landonr"]

NETWORK_TIMEOUT = 30


def has_remote_file_changed(url, local_file_path):
    # Check if the local file exists
    if os.path.exists(local_file_path):
        _LOGGER.warning("has_remote_file_changed: File exists at %s", local_file_path)
        try:
            # Get the local file's modification time
            local_modification_time = os.path.getmtime(local_file_path)

            # Convert it to a format suitable for the "If-Modified-Since" header
            # local_modification_time_str = datetime.utcfromtimestamp(
            #     local_modification_time
            # ).strftime("%a, %d %b %Y %H:%M:%S GMT")

            local_modification_time_str = datetime.utcfromtimestamp(
                local_modification_time
            ).strftime("%a, %d %b %Y %H:%M:%S GMT")

            # Send an HTTP GET request to the URL with the "If-Modified-Since" header
            headers = {
                "If-Modified-Since": local_modification_time_str,
                "Cache-Control": "max-age=3600",
            }
            response = requests.get(url, headers=headers, timeout=NETWORK_TIMEOUT)

            _LOGGER.warning(
                "has_remote_file_changed: File %s, Local modified %s, response code %d",
                local_file_path,
                local_modification_time_str,
                response.status_code,
            )

            if response.status_code == 304:
                _LOGGER.warning(
                    "has_remote_file_changed: File not modified since %s",
                    local_modification_time_str,
                )
                return False
            _LOGGER.warning("has_remote_file_changed: File modified")
            return True
        except requests.exceptions.RequestException as e:
            raise cv.Invalid(
                f"Could check if {url} has changed, please check if file exists "
                f"({e})"
            )

    _LOGGER.warning(
        "has_remote_file_changed: File doesnt exists at %s", local_file_path
    )
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


def get_filename_from_content_disposition(r):
    cd = r.headers.get("content-disposition")
    if not cd:
        return None
    fname = re.findall('filename="(.+)"', cd)
    if len(fname) == 0:
        return None
    file_base_name, file_extension = os.path.splitext(fname[0])
    return file_base_name, file_extension


def parse_file_info_from_url(url):
    parsed_url = urlparse(url)
    path = unquote(parsed_url.path)
    file_name = os.path.basename(path)

    file_base_name, file_extension = os.path.splitext(file_name)
    return file_base_name, file_extension


def get_file_info_from_url(url):
    _LOGGER.warning("get file info url %s", url)
    r = requests.get(url, allow_redirects=True, timeout=NETWORK_TIMEOUT)
    if r.status_code != 200:
        raise cv.Invalid(
            f"Could check {url} info, check if file exists " f"({r.status_code}"
        )
    if "content-disposition" in r.headers:
        file_base_name, file_extension = get_filename_from_content_disposition(r)
        _LOGGER.warning(
            "file at url has content disposition %s %s",
            file_base_name,
            file_extension,
        )
        return file_base_name, file_extension, None
    file_base_name, file_extension = parse_file_info_from_url(url)
    file_name = file_base_name + file_extension
    temp_path = compute_local_file_dir(file_name, "temp")
    _LOGGER.warning("get file info downloading to temp path %s", temp_path)
    with open(temp_path, "wb") as f:
        f.write(r.content)
    return file_base_name, file_extension, temp_path
