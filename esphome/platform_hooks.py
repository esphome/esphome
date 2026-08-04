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
``__main__.py`` stays eager. Both log paths resolve
``process_stacktrace`` lazily through get_stacktrace_handler below.
"""

from __future__ import annotations

from collections.abc import Callable
import importlib
import logging
from typing import Any, Final

from esphome.const import (
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_NRF52,
    PLATFORM_RP2,
    Platform,
)

_LOGGER = logging.getLogger(__name__)

PLATFORM_HOOKS: Final[dict[str, frozenset[str]]] = {
    "show_logs": frozenset({PLATFORM_NRF52}),
    "upload_program": frozenset({PLATFORM_NRF52}),
    "process_stacktrace": frozenset(
        {PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_NRF52, PLATFORM_RP2}
    ),
}


# The registry only speaks for in-tree platforms; a target platform
# supplied via external_components is normally not in Platform and falls
# back to probing the imported package, as the CLI did before the
# registry. Deliberate trade: an external component that shadows an
# in-tree platform name (the meta finder allows it) is treated as the
# in-tree platform here, so its own hooks are not probed.
_IN_TREE_PLATFORMS: Final = frozenset(Platform)


def may_provide_hook(platform: str, hook: str) -> bool:
    """False when the registry proves this in-tree platform lacks the hook.

    External platforms always return True; only the probe can answer for
    them.
    """
    return platform in PLATFORM_HOOKS[hook] or platform not in _IN_TREE_PLATFORMS


def get_platform_hook(platform: str, hook: str) -> Callable[..., Any] | None:
    """Return ``esphome.components.<platform>.<hook>`` or None.

    In-tree platforms not registered for the hook return None without
    being imported. A registered platform that no longer defines the
    hook also returns None, so a stale registry degrades to the generic
    path instead of raising.
    """
    if platform not in PLATFORM_HOOKS[hook]:
        if platform in _IN_TREE_PLATFORMS:
            return None
        # External platform: probe the imported package like the CLI used
        # to. The package can be missing entirely on the warm-cache path,
        # where the external_components meta finder never registered;
        # degrade to the generic path then, but let a failure deeper in
        # the package (missing dependency) surface.
        module_name = f"esphome.components.{platform}"
        try:
            module = importlib.import_module(module_name)
        except ModuleNotFoundError as err:
            if err.name == module_name:
                _LOGGER.debug(
                    "External platform %s is not importable; using the generic %s path",
                    platform,
                    hook,
                )
                return None
            raise
        return getattr(module, hook, None)
    return getattr(
        importlib.import_module(f"esphome.components.{platform}"), hook, None
    )


def get_stacktrace_handler(platform: str) -> Callable[..., Any] | None:
    """Resolve ``process_stacktrace`` for *platform*, degrading with a log.

    Stacktrace decoding is a diagnostic nicety: a platform without an
    analyzer, or one whose package fails to import, must never stop a
    log session. Both callers (serial and API logs) share this so the
    user-facing message lives in one place.
    """
    try:
        handler = get_platform_hook(platform, "process_stacktrace")
    except ImportError as err:
        # A real breakage, not an ordinary capability gap; say so louder.
        _LOGGER.debug("Stacktrace analyzer import failed", exc_info=True)
        _LOGGER.warning(
            'Stacktrace analysis is unavailable: analyzer for target platform "%s" failed to import: %s',
            platform,
            err,
        )
        return None
    if handler is None:
        _LOGGER.info(
            'Stacktrace analysis is unavailable: no compatible analyzer found for target platform "%s".',
            platform,
        )
    return handler
