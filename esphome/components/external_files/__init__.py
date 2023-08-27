from pathlib import Path
import hashlib
import re
import os
from datetime import datetime
from esphome.core import CORE, TimePeriodSeconds

def is_directory_recent(directory_path: str, refresh: TimePeriodSeconds) -> bool:
    if os.path.exists(directory_path):
        current_time = datetime.now().timestamp()
        dir_timestamp = int(os.path.basename(directory_path))
        return current_time - dir_timestamp <= refresh.total_seconds
    return False

def find_recent_directory(base_directory: str, refresh: TimePeriodSeconds) -> Path:
    subdirectories = [d for d in os.listdir(base_directory) if os.path.isdir(os.path.join(base_directory, d))]
    
    for sub_directory in subdirectories:
        if is_directory_recent(os.path.join(base_directory, sub_directory), refresh):
            return os.path.join(base_directory, sub_directory)
    
    return None

def compute_local_file_dir(name: str, domain: str, refresh: TimePeriodSeconds) -> Path:
    base_directory = Path(CORE.config_dir) / ".esphome" / domain / name
    if not os.path.exists(base_directory):
        os.makedirs(base_directory)
    
    recent_directory = find_recent_directory(base_directory, refresh)
    if recent_directory:
        return recent_directory
    
    current_time = int(datetime.now().timestamp())
    sub_directory = Path(os.path.join(base_directory, str(current_time)))
    
    if not os.path.exists(sub_directory):
        os.makedirs(sub_directory)
    
    return sub_directory


def get_file_name_from_url(url):
    # Regular expression pattern to match the file name at the end of the URL
    pattern = r"/([^/]+)$"
    match = re.search(pattern, url)
    if match:
        file_name = match.group(1)
        file_name = re.sub(r"%20", "_", file_name)  # Replace %20 with '_'

        # Remove the file extension
        file_name = re.sub(r"\.[^.]+$", "", file_name)

        return file_name
    return None


def get_file_type(file_name):
    # Regular expression pattern to match the file extension
    pattern = r"\.([^.]+)$"
    match = re.search(pattern, file_name)
    if match:
        file_type = match.group(1)
        return file_type
    return None
