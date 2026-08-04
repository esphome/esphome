"""Registry of platform packages that provide optional CLI hooks.

The logs/upload fast path must know whether a target platform overrides
``show_logs``/``upload_program`` or provides ``process_stacktrace``
without importing the platform package to find out; importing one pulls
in the whole validation stack (config_validation, voluptuous, boards),
which costs seconds on slow hardware. Keep the mapping in sync with the
hook definitions in ``esphome/components/*/__init__.py``; a unit test
imports each platform package and fails when they drift.

The compile-path ``run_compile`` hook is deliberately not registered:
compiling imports the platform package regardless, so its probe in
``__main__.py`` stays eager. The network log client's
``process_stacktrace`` probe in ``esphome/api_client.py`` still uses the
old importlib pattern; converting it is a separate change.
"""

from __future__ import annotations

from collections.abc import Callable
from importlib import import_module
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

# Hooks whose loss only degrades diagnostics; skipping one of these is
# logged at debug, while skipping a hook that changes what the CLI does
# (upload method, log transport) warns. A new hook is loud by default.
COSMETIC_HOOKS: Final = frozenset({"process_stacktrace"})

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


def get_platform_hook(platform: str, hook: str) -> Callable[..., Any] | None:
    """Return ``esphome.components.<platform>.<hook>`` or None.

    In-tree platforms not registered for the hook return None without
    being imported. A registered platform that no longer defines the
    hook also returns None, so a stale registry degrades to the generic
    path instead of raising.
    """
    registered = platform in PLATFORM_HOOKS[hook]
    if not registered and platform in _IN_TREE_PLATFORMS:
        return None
    # For external platforms this probes the imported package like the
    # CLI used to; the package can be missing entirely on the warm-cache
    # path, where the external_components meta finder never registered.
    # Degrade to the generic path then, but let a failure deeper in the
    # package (missing dependency) surface.
    module_name = f"esphome.components.{platform}"
    try:
        module = import_module(module_name)
    except ModuleNotFoundError as err:
        if registered or err.name != module_name:
            raise
        if hook in COSMETIC_HOOKS:
            _LOGGER.debug(
                "External platform %s is not importable; using the generic %s path",
                platform,
                hook,
            )
        else:
            # Deliberately loud even though the warm-cache path makes
            # this expected: the user's platform hooks are not in effect
            # for this run, and a silently substituted upload method is
            # worse than a routine warning.
            _LOGGER.warning(
                "External platform %s is not importable; using the generic %s path",
                platform,
                hook,
            )
        return None
    handler = getattr(module, hook, None)
    if handler is None:
        if registered:
            _LOGGER.warning(
                "%s is registered for %s but no longer exposes it; using the generic path",
                platform,
                hook,
            )
        else:
            # The common case for external platforms; debug so a typoed
            # hook name is still diagnosable without being noisy.
            _LOGGER.debug(
                "External platform %s does not expose %s; using the generic path",
                platform,
                hook,
            )
    return handler
