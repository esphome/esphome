"""Registry of platform packages that provide optional CLI hooks.

The logs/upload fast path must know whether a target platform overrides
``show_logs``/``upload_program`` or provides ``process_stacktrace``
without importing the platform package to find out; importing one pulls
in the whole validation stack (config_validation, voluptuous, boards),
which costs seconds on slow hardware. Keep the mapping in sync with the
hook definitions in ``esphome/components/*/__init__.py``; a unit test
scans the sources and fails when they drift.

The compile-path ``run_compile`` hook is deliberately not registered:
compiling imports the platform package regardless, so its probe in
``__main__.py`` stays eager.
"""

from __future__ import annotations

from collections.abc import Callable
import importlib
from typing import Any, Final

from esphome.const import PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_NRF52, PLATFORM_RP2

PLATFORM_HOOKS: Final[dict[str, frozenset[str]]] = {
    "show_logs": frozenset({PLATFORM_NRF52}),
    "upload_program": frozenset({PLATFORM_NRF52}),
    "process_stacktrace": frozenset(
        {PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_NRF52, PLATFORM_RP2}
    ),
}


def get_platform_hook(platform: str, hook: str) -> Callable[..., Any] | None:
    """Return ``esphome.components.<platform>.<hook>`` or None.

    Platforms not registered for the hook return None without being
    imported. A registered platform that no longer defines the hook also
    returns None, so a stale registry degrades to the generic path
    instead of raising.
    """
    if platform not in PLATFORM_HOOKS[hook]:
        return None
    return getattr(
        importlib.import_module(f"esphome.components.{platform}"), hook, None
    )
