"""Run the esp32 storage fast path and report which heavy modules loaded.

Executed as a subprocess by test_lazy_imports.py: heavy module names come
in on argv, the ones found in sys.modules afterwards go out on stdout.
"""

import sys

from _leak_report import print_leaked_modules
from _storage import make_storage

from esphome.const import KEY_ESP32, KEY_IDF_VERSION, KEY_VARIANT
from esphome.core import CORE, Version

make_storage().apply_to_core()

# Fail loudly if the esp32 fast path stopped doing its work; otherwise an
# empty leak list could just mean nothing ran. Explicit exits rather than
# asserts so PYTHONOPTIMIZE in the ambient environment can't strip them.
esp32_data = CORE.data.get(KEY_ESP32, {})
if esp32_data.get(KEY_VARIANT) != "ESP32S3":
    sys.exit(f"apply_to_core did not record the variant: {esp32_data!r}")
if esp32_data.get(KEY_IDF_VERSION) != Version(5, 3, 1):
    sys.exit(f"apply_to_core did not parse the framework version: {esp32_data!r}")

print_leaked_modules()
