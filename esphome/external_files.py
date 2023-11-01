import logging
from pathlib import Path
import os
from urllib.parse import urlparse, unquote
from datetime import datetime
import requests
import esphome.config_validation as cv
from esphome.core import CORE, TimePeriodSeconds


_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@landonr"]

NETWORK_TIMEOUT = 30


def has_remote_file_changed(url, local_file_path):
    # Check if the local file exists
    if os.path.exists(local_file_path):
        _LOGGER.debug("File exists at %s", local_file_path)
        try:
            # Get the local file's modification time
            local_modification_time = os.path.getmtime(local_file_path)

            # Convert it to a format suitable for the "If-Modified-Since" header
            local_modification_time_str = datetime.utcfromtimestamp(
                local_modification_time
            ).strftime("%a, %d %b %Y %H:%M:%S GMT")

            # Send an HTTP GET request to the URL with the "If-Modified-Since" header
            headers = {"If-Modified-Since": local_modification_time_str}
            response = requests.get(url, headers=headers, timeout=NETWORK_TIMEOUT)

            _LOGGER.debug(
                "File %s, Local modified %s, response code %d",
                local_file_path,
                local_modification_time_str,
                response.status_code,
            )
            # Check if the response indicates that the file has been modified
            return response.status_code == 200
        except requests.exceptions.RequestException as e:
            raise cv.Invalid(
                f"Could check if {url} has changed, please check if file exists "
                f"({e})"
            )

    _LOGGER.warning("File doesnt exists at %s", local_file_path)
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


def get_file_info_from_url(url):
    # Regular expression pattern to match the file name at the end of the URL
    parsed_url = urlparse(url)

    # Get the path component
    path = unquote(parsed_url.path)

    # Extract file name and extension
    file_name = os.path.basename(path)

    file_base_name, file_extension = os.path.splitext(file_name)
    return file_base_name, file_extension
