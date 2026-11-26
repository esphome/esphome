"""Feed a serial capture through the CLI's stacktrace decoder for a build.

usage: decode_capture.py CONFIG_YAML CAPTURE

Run from the directory that holds the config (its .esphome build is used), with the esphome checkout that
built it on PYTHONPATH. Lines of the crash report are echoed, decoded frames follow them.
"""

import logging
from pathlib import Path
import sys

from esphome.config import read_config
from esphome.core import CORE
from esphome.stacktrace import LogLineProcessor

logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
yaml, capture = Path(sys.argv[1]), Path(sys.argv[2])
CORE.config_path = yaml
config = read_config({})
proc = LogLineProcessor(config, CORE.target_platform)
for line in capture.read_text(encoding="utf-8", errors="replace").splitlines():
    if "libretiny.crash" in line:
        print(line[:120])
    proc.process_line(line)
