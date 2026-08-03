"""Guard the platform CLI-hook registry in ``esphome.platform_hooks``.

The registry lets the logs/upload fast path skip importing platform
packages that don't provide a hook; these tests fail when a platform
gains or loses a hook without the registry being updated, and pin down
that the fast path really avoids the import.
"""

from __future__ import annotations

from pathlib import Path
import re
import subprocess
import sys

from esphome import platform_hooks

COMPONENTS_DIR = Path(platform_hooks.__file__).parent / "components"

HOOKS = {
    "show_logs": platform_hooks.PLATFORMS_WITH_SHOW_LOGS,
    "upload_program": platform_hooks.PLATFORMS_WITH_UPLOAD_PROGRAM,
    "process_stacktrace": platform_hooks.PLATFORMS_WITH_PROCESS_STACKTRACE,
}

_HOOK_DEF_RE = re.compile(
    r"^def (show_logs|upload_program|process_stacktrace)\(", re.MULTILINE
)


def _scan_hook_definitions() -> dict[str, set[str]]:
    """Find module-level hook definitions in every component package."""
    found: dict[str, set[str]] = {hook: set() for hook in HOOKS}
    for init in COMPONENTS_DIR.glob("*/__init__.py"):
        for match in _HOOK_DEF_RE.finditer(init.read_text()):
            found[match.group(1)].add(init.parent.name)
    return found


def test_registry_matches_component_sources() -> None:
    """Every hook definition must be registered, and vice versa."""
    found = _scan_hook_definitions()
    for hook, registered in HOOKS.items():
        assert found[hook] == set(registered), (
            f"platform_hooks registry for {hook} is out of sync with "
            f"esphome/components/*/__init__.py: sources define {sorted(found[hook])}, "
            f"registry has {sorted(registered)}. Update esphome/platform_hooks.py."
        )


def test_unregistered_platform_returns_none() -> None:
    assert (
        platform_hooks.get_platform_hook(
            "esp32", "show_logs", platform_hooks.PLATFORMS_WITH_SHOW_LOGS
        )
        is None
    )


def test_registered_platform_resolves_hook() -> None:
    hook = platform_hooks.get_platform_hook(
        "esp32",
        "process_stacktrace",
        platform_hooks.PLATFORMS_WITH_PROCESS_STACKTRACE,
    )
    from esphome.components import esp32

    assert hook is esp32.process_stacktrace


def test_lookup_miss_does_not_import_platform_package() -> None:
    """The whole point: probing a platform without hooks must not import it."""
    check = (
        "import sys; "
        "from esphome.platform_hooks import PLATFORMS_WITH_SHOW_LOGS, get_platform_hook; "
        "assert get_platform_hook('esp32', 'show_logs', PLATFORMS_WITH_SHOW_LOGS) is None; "
        "leaked = [m for m in sys.modules if m.startswith('esphome.components.')]; "
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
        f"get_platform_hook imported platform packages on a registry miss: {leaked}"
    )
