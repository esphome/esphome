"""Registry of platform packages that provide optional CLI hooks.

The logs/upload fast path must know whether a target platform overrides
``show_logs``/``upload_program`` or provides ``process_stacktrace``
without importing the platform package to find out; importing one pulls
in the whole validation stack (config_validation, voluptuous, boards),
which costs seconds on slow hardware. Keep these sets in sync with the
``def show_logs``/``def upload_program``/``def process_stacktrace``
definitions in ``esphome/components/*/__init__.py``; a unit test scans
the sources and fails when they drift.
"""

from __future__ import annotations

from collections.abc import Callable
import importlib
from typing import Any, Final

from esphome.const import PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_NRF52, PLATFORM_RP2

PLATFORMS_WITH_SHOW_LOGS: Final = frozenset({PLATFORM_NRF52})
PLATFORMS_WITH_UPLOAD_PROGRAM: Final = frozenset({PLATFORM_NRF52})
PLATFORMS_WITH_PROCESS_STACKTRACE: Final = frozenset(
    {PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_NRF52, PLATFORM_RP2}
)


def get_platform_hook(
    platform: str, hook: str, platforms: frozenset[str]
) -> Callable[..., Any] | None:
    """Return ``esphome.components.<platform>.<hook>`` or None.

    Platforms not in the registry return None without being imported.
    """
    if platform not in platforms:
        return None
    return getattr(importlib.import_module(f"esphome.components.{platform}"), hook)
