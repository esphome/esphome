"""Run the upload command dispatch path and report which heavy modules loaded.

Executed as a subprocess by test_lazy_imports.py: heavy module names come
in on argv, the ones found in sys.modules afterwards go out on stdout.
Covers both fast-path claims: the bundle suffix check in run_esphome reads
BUNDLE_EXTENSION from esphome.const without importing esphome.bundle, and
the real validated-config cache parse, include resolution included, stays
voluptuous free.
"""

import os
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

from _leak_report import print_leaked_modules

from esphome import __main__ as main_mod
from esphome.storage_json import StorageJSON

# The watch list rides on argv for _leak_report; strip it before
# argparse sees it.
watch_list = sys.argv[1:]
sys.argv = [sys.argv[0]]

# An ambient data-dir override would relocate the storage tree away
# from the tmp config dir this fixture builds.
os.environ.pop("ESPHOME_DATA_DIR", None)
os.environ.pop("ESPHOME_IS_HA_ADDON", None)

tmp = Path(tempfile.mkdtemp())
conf_path = tmp / "test.yaml"
conf_path.write_text("esphome:\n  name: t\n")

storage_dir = tmp / ".esphome" / "storage"
storage_dir.mkdir(parents=True)
# The cache is a top-level !include so loading it resolves an
# IncludeFile for real on the fast path.
(storage_dir / "inc.yaml").write_text("esphome:\n  name: t\n")
cache_path = storage_dir / "test.yaml.validated.yaml"
cache_path.write_text("!include inc.yaml\n")
os.utime(cache_path)  # keep the cache at least as fresh as the source

storage = StorageJSON(
    storage_version=1,
    name="t",
    friendly_name="T",
    comment=None,
    esphome_version="2026.1.0",
    src_version=1,
    address="1.2.3.4",
    web_port=None,
    target_platform="ESP32",
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

dispatched = {}


def fake_upload(args, config):
    dispatched["config"] = config
    return 0


# Written to the layout ext_storage_path resolves once run_esphome
# sets CORE.config_path; going through CORE here would be circular.
storage.save(storage_dir / "test.yaml.json")

with patch.dict(main_mod.POST_CONFIG_ACTIONS, {"upload": fake_upload}):
    exit_code = main_mod.run_esphome(
        ["esphome", "upload", str(conf_path), "--device", "192.0.2.1"]
    )

# Fail loudly if the fast path didn't do its work; otherwise an empty
# leak list could just mean nothing ran. Explicit exits rather than
# asserts so PYTHONOPTIMIZE in the ambient environment can't strip them.
if exit_code != 0:
    sys.exit(f"run_esphome exited {exit_code} before dispatching upload")
if dispatched.get("config") != {"esphome": {"name": "t"}}:
    sys.exit(f"cache include did not resolve through the fast path: {dispatched!r}")

sys.argv = [sys.argv[0], *watch_list]
print_leaked_modules()
