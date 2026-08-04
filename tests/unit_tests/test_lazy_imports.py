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

import importlib.util
import os
from pathlib import Path
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

# Everything the storage fast path must keep out of sys.modules; the
# existence guard and the leak check must watch the same list.
FAST_PATH_HEAVY_MODULES = HEAVY_MODULES + ("esphome.components.esp32",)


def _leaked_heavy_modules(module: str) -> str:
    """Import ``module`` in a subprocess and report the heavy modules it pulled.

    Any ``esphome.components.*`` package counts as heavy: executing a
    component package drags in codegen/validation machinery by design.
    """
    check = (
        f"import sys; import {module}; "
        f"leaked = [m for m in {HEAVY_MODULES!r} if m in sys.modules]; "
        "leaked += [m for m in sys.modules if m.startswith('esphome.components.')]; "
        "print(','.join(leaked))"
    )
    result = subprocess.run(
        [sys.executable, "-c", check],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout.strip()


def test_main_module_does_not_import_heavy_modules() -> None:
    """A bare ``import esphome.__main__`` must not drag in validation/codegen."""
    leaked = _leaked_heavy_modules("esphome.__main__")
    assert not leaked, (
        f"esphome.__main__ imports heavy modules at top level: {leaked}. "
        "Import them lazily inside the command that needs them instead; "
        "every esphome invocation (including each parallel dashboard "
        "upload subprocess) pays for top-level imports."
    )


def test_watched_heavy_modules_exist() -> None:
    """A renamed heavy module would silently disable the leak checks."""
    for module in FAST_PATH_HEAVY_MODULES:
        assert importlib.util.find_spec(module) is not None, (
            f"{module} no longer resolves; update the heavy-module lists"
        )


def test_storage_json_fast_path_does_not_import_heavy_modules(
    fixture_path: Path,
) -> None:
    """``apply_to_core`` runs on the upload/logs fast path for every
    platform; parsing the stored framework version must not drag in the
    validation stack or the esp32 component package.
    """
    script = fixture_path / "lazy_imports" / "storage_json_fast_path.py"
    # Running a script file drops the cwd from sys.path, so prepend the
    # repo root for the child; check=False keeps its stderr visible.
    python_path = str(Path(__file__).parents[2])
    if ambient := os.environ.get("PYTHONPATH"):
        python_path = os.pathsep.join((python_path, ambient))
    env = os.environ | {"PYTHONPATH": python_path}
    result = subprocess.run(
        [sys.executable, str(script), *FAST_PATH_HEAVY_MODULES],
        capture_output=True,
        text=True,
        env=env,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    leaked = result.stdout.strip()
    assert not leaked, (
        f"storage_json.apply_to_core pulls in heavy modules: {leaked}. "
        "The upload/logs fast path skips validation; importing the "
        "validation stack anyway defeats the validated-config cache."
    )


def test_api_client_does_not_import_heavy_modules() -> None:
    """``esphome.api_client`` is on the logs fast path and must stay light.

    Importing it must not execute any component package (the api package
    pulls the whole validation stack: logger, esp32, writer, config,
    jinja2, voluptuous).
    """
    leaked = _leaked_heavy_modules("esphome.api_client")
    assert not leaked, (
        f"esphome.api_client imports heavy modules at top level: {leaked}. "
        "The logs fast path skips validation; importing the validation "
        "stack anyway defeats the validated-config cache."
    )
