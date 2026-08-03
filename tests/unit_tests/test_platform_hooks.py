"""Guard the platform CLI-hook registry in ``esphome.platform_hooks``.

The registry lets the logs/upload fast path skip importing platform
packages that don't provide a hook; these tests fail when a platform
gains or loses a hook without the registry being updated, and pin down
that the fast path really avoids the import.
"""

from __future__ import annotations

from pathlib import Path
import re
from unittest.mock import Mock

import pytest

from esphome import platform_hooks
from esphome.const import PLATFORM_ESP32

COMPONENTS_DIR = Path(platform_hooks.__file__).parent / "components"

_HOOK_DEF_RE = re.compile(
    rf"^def ({'|'.join(platform_hooks.PLATFORM_HOOKS)})\(", re.MULTILINE
)


def _scan_hook_definitions() -> dict[str, set[str]]:
    """Find module-level hook definitions in every component package."""
    found: dict[str, set[str]] = {hook: set() for hook in platform_hooks.PLATFORM_HOOKS}
    for init in COMPONENTS_DIR.glob("*/__init__.py"):
        for match in _HOOK_DEF_RE.finditer(init.read_text(encoding="utf-8")):
            found[match.group(1)].add(init.parent.name)
    return found


def test_registry_matches_component_sources() -> None:
    """Every hook definition must be registered, and vice versa."""
    found = _scan_hook_definitions()
    for hook, registered in platform_hooks.PLATFORM_HOOKS.items():
        assert found[hook] == set(registered), (
            f"platform_hooks registry for {hook} is out of sync with "
            f"esphome/components/*/__init__.py: sources define {sorted(found[hook])}, "
            f"registry has {sorted(registered)}. Update esphome/platform_hooks.py."
        )


def test_registered_platform_resolves_hook() -> None:
    hook = platform_hooks.get_platform_hook(PLATFORM_ESP32, "process_stacktrace")
    from esphome.components import esp32

    assert hook is esp32.process_stacktrace


def test_lookup_miss_does_not_import_platform_package(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The whole point: probing a platform without hooks must not import it."""
    monkeypatch.setattr(
        platform_hooks.importlib,
        "import_module",
        Mock(side_effect=AssertionError("platform package imported on registry miss")),
    )
    assert platform_hooks.get_platform_hook(PLATFORM_ESP32, "show_logs") is None
