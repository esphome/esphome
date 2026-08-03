"""Run the esp32 storage fast path and report which heavy modules loaded.

Executed as a subprocess by test_lazy_imports.py: heavy module names come
in on argv, the ones found in sys.modules afterwards go out on stdout.
"""

import sys

from esphome.storage_json import StorageJSON

storage = StorageJSON(
    storage_version=1,
    name="test",
    friendly_name="Test",
    comment=None,
    esphome_version="2026.1.0",
    src_version=1,
    address="1.2.3.4",
    web_port=None,
    target_platform="ESP32S3",
    build_path=None,
    firmware_bin_path=None,
    loaded_integrations=set(),
    loaded_platforms=set(),
    no_mdns=False,
    framework="esp-idf",
    core_platform="esp32",
    area=None,
    framework_version="5.3.1",
)
storage.apply_to_core()

print(",".join(module for module in sys.argv[1:] if module in sys.modules))
