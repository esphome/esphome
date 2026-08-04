"""Run the esptool serial-upload path and report which heavy modules loaded.

Executed as a subprocess by test_lazy_imports.py: heavy module names come
in on argv, the ones found in sys.modules afterwards go out on stdout.
The variant reaches the esptool command line from CORE.data directly; if
someone re-adds the esp32 package import for it, this reports the leak.
"""

from pathlib import Path
import sys
import tempfile
from unittest.mock import MagicMock, patch

from esphome.__main__ import upload_using_esptool
from esphome.const import (
    CONF_ESPHOME,
    KEY_CORE,
    KEY_ESP32,
    KEY_TARGET_PLATFORM,
    KEY_VARIANT,
)
from esphome.core import CORE

tmp = Path(tempfile.mkdtemp())
CORE.config_path = tmp / "test.yaml"
CORE.name = "test"
CORE.build_path = tmp
CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
CORE.data[KEY_ESP32] = {KEY_VARIANT: "ESP32S3"}
(tmp / "firmware.bin").touch()

idedata = MagicMock()
idedata.firmware_bin_path = tmp / "firmware.bin"
idedata.extra_flash_images = []

with (
    patch("esphome.platformio.toolchain.get_idedata", return_value=idedata),
    patch("esphome.__main__.run_external_command", return_value=0) as mock_run,
):
    rc = upload_using_esptool(
        {CONF_ESPHOME: {"platformio_options": {}}}, "/dev/ttyUSB0", None, None
    )

# Fail loudly if the upload path stopped doing its work; otherwise an
# empty leak list could just mean nothing ran.
if rc != 0:
    sys.exit(f"upload_using_esptool returned {rc}")
cmd = list(mock_run.call_args[0][1:])
if cmd[cmd.index("--chip") + 1] != "esp32s3":
    sys.exit(f"variant did not reach the esptool command line: {cmd}")

leaked = [module for module in sys.argv[1:] if module in sys.modules]
leaked += [
    module
    for module in sys.modules
    if module.startswith("esphome.components.") and module not in leaked
]
print(",".join(leaked))
