"""Run the esptool serial-upload path and report which heavy modules loaded.

Executed as a subprocess by test_lazy_imports.py: heavy module names come
in on argv, the ones found in sys.modules afterwards go out on stdout.
The variant reaches the esptool command line from CORE.data directly; if
someone re-adds the esp32 package import for it, this reports the leak.
"""

import os
import sys
from unittest.mock import patch

from _leak_report import print_leaked_modules

from esphome.__main__ import upload_using_esptool
from esphome.const import (
    CONF_ESPHOME,
    KEY_CORE,
    KEY_ESP32,
    KEY_TARGET_PLATFORM,
    KEY_VARIANT,
)
from esphome.core import CORE

# An ambient ESPHOME_USE_SUBPROCESS would route past the patched
# run_external_command into run_external_process and confuse the checks.
os.environ.pop("ESPHOME_USE_SUBPROCESS", None)

CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
CORE.data[KEY_ESP32] = {KEY_VARIANT: "ESP32S3"}

with patch("esphome.__main__.run_external_command", return_value=0) as mock_run:
    rc = upload_using_esptool(
        {CONF_ESPHOME: {"platformio_options": {}}}, "/dev/ttyUSB0", "firmware.bin", None
    )

# Fail loudly if the upload path stopped doing its work; otherwise an
# empty leak list could just mean nothing ran.
if rc != 0:
    sys.exit(f"upload_using_esptool returned {rc}")
cmd = list(mock_run.call_args[0][1:])
if cmd[cmd.index("--chip") + 1] != "esp32s3":
    sys.exit(f"variant did not reach the esptool command line: {cmd}")

print_leaked_modules()
