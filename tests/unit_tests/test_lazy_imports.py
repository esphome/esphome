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

# Heavy only for modules that must not know about the API transport;
# in the existence guard so a rename can't silently no-op its check.
API_HEAVY_MODULES = ("aioesphomeapi",)

# Heavy only for the single-config dispatch path: the bundle suffix
# check reads BUNDLE_EXTENSION from esphome.const so an ordinary run
# never pays for the bundle machinery and its tarfile chain.
BUNDLE_HEAVY_MODULES = ("esphome.bundle", "tarfile")

# Heavy only for a cache-hit upload/logs run: the JSON cache parse must
# not resolve pyyaml or the yaml_util chain (the read_config fallback
# still uses both).
CACHE_HIT_HEAVY_MODULES = ("esphome.yaml_util", "yaml")

# Stdlib modules deferred out of the dispatch fast path: a cache-hit
# upload/logs run never writes a file (tempfile), spawns a process
# (subprocess), parses a URL (urllib.parse), or prints a serial
# permission hint (getpass). shutil is deferred too but unwatchable:
# argparse imports it from every add_argument on py3.14. urllib.parse
# is only watchable on 3.13+ where pathlib stopped importing it.
STDLIB_FAST_PATH_MODULES = (
    "tempfile",
    "subprocess",
    "getpass",
    "datetime",
    *(("urllib.parse",) if sys.version_info >= (3, 13) else ()),
)


def _leaked_heavy_modules(module: str, extra: tuple[str, ...] = ()) -> str:
    """Import ``module`` in a subprocess and report the heavy modules it pulled.

    Any ``esphome.components.*`` package counts as heavy: executing a
    component package drags in codegen/validation machinery by design.
    ``extra`` adds modules that are heavy for this caller specifically.
    """
    check = (
        f"import sys; import {module}; "
        f"leaked = [m for m in {HEAVY_MODULES + extra!r} if m in sys.modules]; "
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
    """A bare ``import esphome.__main__`` must not drag in validation/codegen.

    The stdlib watch list rides along here because this check runs in a
    clean subprocess: a module-level re-import anywhere on the chain is
    caught, which the dispatch fixture (whose setup pre-imports them and
    pops before dispatch) structurally cannot do.
    """
    leaked = _leaked_heavy_modules("esphome.__main__", extra=STDLIB_FAST_PATH_MODULES)
    assert not leaked, (
        f"esphome.__main__ imports heavy modules at top level: {leaked}. "
        "Import them lazily inside the command that needs them instead; "
        "every esphome invocation (including each parallel dashboard "
        "upload subprocess) pays for top-level imports."
    )


def test_watched_heavy_modules_exist() -> None:
    """A renamed heavy module would silently disable the leak checks."""
    for module in (
        FAST_PATH_HEAVY_MODULES
        + API_HEAVY_MODULES
        + BUNDLE_HEAVY_MODULES
        + CACHE_HIT_HEAVY_MODULES
        + STDLIB_FAST_PATH_MODULES
    ):
        assert importlib.util.find_spec(module) is not None, (
            f"{module} no longer resolves; update the heavy-module lists"
        )


def _leaked_from_fixture(
    fixture_path: Path,
    env: dict[str, str],
    script_name: str,
    extra: tuple[str, ...] = (),
) -> str:
    """Run a fixture script with the watched modules on argv.

    ``env`` comes from the ``probe_env`` fixture so the child can import
    the repo checkout; a non-zero exit surfaces the child's stderr.
    """
    script = fixture_path / "lazy_imports" / script_name
    result = subprocess.run(
        [sys.executable, str(script), *FAST_PATH_HEAVY_MODULES, *extra],
        capture_output=True,
        text=True,
        env=env,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    return result.stdout.strip()


def test_storage_json_fast_path_does_not_import_heavy_modules(
    fixture_path: Path,
    probe_env: dict[str, str],
) -> None:
    """``apply_to_core`` runs on the upload/logs fast path for every
    platform; parsing the stored framework version must not drag in the
    validation stack or the esp32 component package.
    """
    leaked = _leaked_from_fixture(fixture_path, probe_env, "storage_json_fast_path.py")
    assert not leaked, (
        f"storage_json.apply_to_core pulls in heavy modules: {leaked}. "
        "The upload/logs fast path skips validation; importing the "
        "validation stack anyway defeats the validated-config cache."
    )


def test_esptool_upload_fast_path_does_not_import_heavy_modules(
    fixture_path: Path,
    probe_env: dict[str, str],
) -> None:
    """The esptool serial upload reads the esp32 variant from CORE.data;
    resolving it must not drag in the esp32 component package or the
    validation stack.
    """
    leaked = _leaked_from_fixture(
        fixture_path, probe_env, "esptool_upload_fast_path.py"
    )
    assert not leaked, (
        f"upload_using_esptool pulls in heavy modules: {leaked}. "
        "The upload fast path skips validation; importing the validation "
        "stack anyway defeats the validated-config cache."
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


def test_stacktrace_does_not_import_heavy_modules() -> None:
    """``esphome.stacktrace`` guards its own docstring's contract.

    Both log paths construct a LogLineProcessor before streaming
    starts; importing the module must not pull in aioesphomeapi or
    any platform package.
    """
    leaked = _leaked_heavy_modules("esphome.stacktrace", extra=API_HEAVY_MODULES)
    assert not leaked, (
        f"esphome.stacktrace imports heavy modules at top level: {leaked}. "
        "The logs fast path skips validation; importing the validation "
        "stack anyway defeats the validated-config cache."
    )


def test_espidf_toolchain_does_not_import_heavy_modules() -> None:
    """The esp-idf upload path must not pull the esp32 package back in.

    upload_using_esptool reaches espidf.toolchain for esp-idf builds;
    its keys and the variant mapping live in esphome.const and
    esphome.espidf precisely so this import stays light.
    """
    leaked = _leaked_heavy_modules("esphome.espidf.toolchain")
    assert not leaked, (
        f"esphome.espidf.toolchain imports heavy modules: {leaked}. "
        "The upload fast path skips validation; importing the validation "
        "stack anyway defeats the validated-config cache."
    )


def test_has_mqtt_ip_lookup_does_not_import_mqtt() -> None:
    """``has_mqtt_ip_lookup`` runs on the upload/logs fast path for mqtt
    configs; reading ``CONF_DISCOVER_IP`` must not drag in the mqtt
    component and, with it, the validation stack.

    Runs in a subprocess because this session's other tests import the
    mqtt component; the fast path itself must not.
    """
    check = (
        "import sys; from esphome.__main__ import has_mqtt_ip_lookup; "
        "from esphome.core import CORE; from esphome.const import CONF_MQTT; "
        "CORE.config = {CONF_MQTT: {}}; "
        "assert has_mqtt_ip_lookup() is True, 'mqtt IP lookup default broke'; "
        f"leaked = [m for m in {HEAVY_MODULES!r} if m in sys.modules]; "
        "leaked += [m for m in sys.modules if m.startswith('esphome.components.')]; "
        "print(','.join(leaked))"
    )
    # check=False keeps the child's stderr (its assertion message or an
    # import traceback) visible on failure.
    result = subprocess.run(
        [sys.executable, "-c", check],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    leaked = result.stdout.strip()
    assert not leaked, (
        f"has_mqtt_ip_lookup pulls in heavy modules: {leaked}. "
        "The upload/logs fast path skips validation; importing the "
        "validation stack anyway defeats the validated-config cache."
    )


def test_yaml_util_does_not_import_heavy_modules() -> None:
    """``esphome.yaml_util`` parses the validated-config cache on the
    upload/logs fast path; importing it must not pull in voluptuous.
    """
    leaked = _leaked_heavy_modules("esphome.yaml_util")
    assert not leaked, (
        f"esphome.yaml_util imports heavy modules at top level: {leaked}. "
        "The upload/logs fast path skips validation; importing the "
        "validation stack anyway defeats the validated-config cache."
    )


def test_upload_command_path_does_not_import_heavy_modules(
    fixture_path: Path,
    probe_env: dict[str, str],
) -> None:
    """The single-config dispatch path checks the bundle suffix on every
    run; reading it from esphome.const must not drag in esphome.bundle
    and its tarfile chain.
    """
    leaked = _leaked_from_fixture(
        fixture_path,
        probe_env,
        "upload_command_fast_path.py",
        extra=BUNDLE_HEAVY_MODULES + CACHE_HIT_HEAVY_MODULES + STDLIB_FAST_PATH_MODULES,
    )
    assert not leaked, (
        f"the upload dispatch path pulls in heavy modules: {leaked}. "
        "An ordinary run only needs the bundle suffix constant, and the "
        "JSON cache parse must not resolve voluptuous or pyyaml; keep the "
        "esphome.bundle import inside the branch that extracts one, the "
        "yaml_util imports inside the read_config fallback, and the "
        "deferred stdlib imports inside the write/spawn/serial helpers."
    )
