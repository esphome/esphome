from pathlib import Path
import os
from urllib.parse import urlparse, unquote
from datetime import datetime
import requests
from esphome.core import CORE, TimePeriodSeconds

CODEOWNERS = ["@landonr"]

NETWORK_TIMEOUT = 30
ETAG = "ETag"


def check_etag_equality(url, local_file_path):
    # Check if the local file exists
    if os.path.exists(local_file_path):
        # Send an HTTP HEAD request to the URL to get the remote file's ETag
        response = requests.head(url, timeout=NETWORK_TIMEOUT)
        remote_etag = response.headers.get(ETAG)

        # Get the local file's ETag if available
        with open(local_file_path, "rb") as local_file:
            local_etag = local_file.headers.get(ETAG)

        # Compare ETags
        if remote_etag is not None and local_etag is not None:
            return remote_etag == local_etag
    return False


def is_file_recent(file_path: str, refresh: TimePeriodSeconds) -> bool:
    if os.path.exists(file_path):
        creation_time = os.path.getctime(file_path)
        current_time = datetime.now().timestamp()
        return current_time - creation_time <= refresh.total_seconds
    return False


def compute_local_file_dir(name: str, domain: str) -> Path:
    base_directory = Path(CORE.config_dir) / ".esphome" / domain
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
