from pathlib import Path
import os
from urllib.parse import urlparse, unquote
from datetime import datetime
from esphome.core import CORE, TimePeriodSeconds

CODEOWNERS = ["@landonr"]


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
