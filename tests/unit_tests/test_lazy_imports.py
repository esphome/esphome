"""Guard the lazy-import contract of ``esphome.__main__``.

Every ``esphome`` invocation pays for whatever ``esphome.__main__``
imports at module level before the requested command runs. The
dashboard and device-builder spawn one ``esphome upload`` subprocess
per device, so keeping validation/codegen machinery out of the
top-level import directly lowers the RAM cost of each concurrent
upload (the upload/logs fast path in ``esphome.compiled_config``
never needs them).

``script/check_import_time.py`` budgets import *time* in CI; this
test pins down *which* heavy modules must stay out entirely.
"""

from __future__ import annotations

import subprocess
import sys

# Modules that must only load for the commands that actually use them
# (compile/config validation, shell completion), never from a bare
# ``import esphome.__main__``.
HEAVY_MODULES = (
    "argcomplete",
    "esphome.codegen",
    "esphome.config",
    "esphome.config_validation",
    "esphome.cpp_generator",
    "esphome.loader",
    "voluptuous",
)


def test_main_module_does_not_import_heavy_modules() -> None:
    """A bare ``import esphome.__main__`` must not drag in validation/codegen."""
    check = (
        "import sys; import esphome.__main__; "
        f"leaked = [m for m in {HEAVY_MODULES!r} if m in sys.modules]; "
        "print(','.join(leaked))"
    )
    result = subprocess.run(
        [sys.executable, "-c", check],
        capture_output=True,
        text=True,
        check=True,
    )
    leaked = result.stdout.strip()
    assert not leaked, (
        f"esphome.__main__ imports heavy modules at top level: {leaked}. "
        "Import them lazily inside the command that needs them instead; "
        "every esphome invocation (including each parallel dashboard "
        "upload subprocess) pays for top-level imports."
    )


def test_storage_json_fast_path_does_not_import_heavy_modules() -> None:
    """``apply_to_core`` runs on the upload/logs fast path for every
    platform; parsing the stored framework version must not drag in the
    validation stack or the esp32 component package.
    """
    heavy = HEAVY_MODULES + ("esphome.components.esp32",)
    check = (
        "import sys\n"
        "from esphome.storage_json import StorageJSON\n"
        "storage = StorageJSON(\n"
        "    storage_version=1, name='test', friendly_name='Test', comment=None,\n"
        "    esphome_version='2026.1.0', src_version=1, address='1.2.3.4',\n"
        "    web_port=None, target_platform='ESP32S3', build_path=None,\n"
        "    firmware_bin_path=None, loaded_integrations=set(),\n"
        "    loaded_platforms=set(), no_mdns=False, framework='esp-idf',\n"
        "    core_platform='esp32', area=None, framework_version='5.3.1',\n"
        ")\n"
        "storage.apply_to_core()\n"
        f"leaked = [m for m in {heavy!r} if m in sys.modules]\n"
        "print(','.join(leaked))\n"
    )
    result = subprocess.run(
        [sys.executable, "-c", check],
        capture_output=True,
        text=True,
        check=True,
    )
    leaked = result.stdout.strip()
    assert not leaked, (
        f"storage_json.apply_to_core pulls in heavy modules: {leaked}. "
        "The upload/logs fast path skips validation; importing the "
        "validation stack anyway defeats the validated-config cache."
    )
