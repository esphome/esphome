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
