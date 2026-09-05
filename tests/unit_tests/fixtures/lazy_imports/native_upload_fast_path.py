"""Run the native-toolchain serial-upload path and report heavy modules.

Executed as a subprocess by test_lazy_imports.py: heavy module names come
in on argv, the ones found in sys.modules afterwards go out on stdout.
``upload_using_esptool`` dispatches native toolchains through a
toolchain-keyed table; if someone routes it back through the platform
component packages (esp32 or esp8266), this reports the leak.
"""

import os
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

from _leak_report import print_leaked_modules

from esphome.__main__ import upload_using_esptool
from esphome.const import (
    CONF_ESPHOME,
    KEY_CORE,
    KEY_ESP32,
    KEY_TARGET_PLATFORM,
    KEY_VARIANT,
    Toolchain,
)
from esphome.core import CORE

# An ambient ESPHOME_USE_SUBPROCESS would route past the patched
# run_external_command into run_external_process and confuse the checks.
os.environ.pop("ESPHOME_USE_SUBPROCESS", None)

config = {CONF_ESPHOME: {"platformio_options": {}}}

with tempfile.TemporaryDirectory() as build_dir:
    CORE.name = "leaktest"
    CORE.build_path = build_dir

    for platform, toolchain, backend in (
        ("esp8266", Toolchain.ARDUINO, "esphome.arduino8266.toolchain"),
        ("esp32", Toolchain.ESP_IDF, "esphome.espidf.toolchain"),
    ):
        CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
        if platform == "esp32":
            CORE.data[KEY_ESP32] = {KEY_VARIANT: "ESP32S3"}
        CORE.toolchain = toolchain

        import importlib

        image = importlib.import_module(backend).get_factory_firmware_path()
        image.parent.mkdir(parents=True, exist_ok=True)
        image.write_bytes(b"\x00")

        with patch("esphome.__main__.run_external_command", return_value=0) as mock_run:
            rc = upload_using_esptool(config, "/dev/ttyUSB0", None, None)

        # Fail loudly if the upload path stopped doing its work; otherwise
        # an empty leak list could just mean nothing ran.
        if rc != 0:
            sys.exit(f"upload_using_esptool({platform}) returned {rc}")
        cmd = list(mock_run.call_args[0][1:])
        if str(image) not in [str(Path(c)) for c in cmd]:
            sys.exit(f"native factory image did not reach esptool: {cmd}")

print_leaked_modules()
