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
``__main__.py`` stays eager. Both log paths resolve
``process_stacktrace`` through ``esphome.stacktrace.LogLineProcessor``,
which uses get_stacktrace_handler below.
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

# Per-platform trigger languages for lazy stacktrace decoding: a
# matching line is what imports the platform package, so false triggers
# (8-digit uptime counters, ESP-IDF decimal timestamps) must stay out.
# Declaring a gate registers the process_stacktrace hook, and each gate
# must stay a superset of its decoder patterns' trigger language; both
# are enforced by tests/unit_tests/test_stacktrace.py. Stored as
# strings so a log session compiles only its own platform's gate.
STACKTRACE_GATES: Final[dict[str, str]] = {
    PLATFORM_ESP32: (
        r"0x[0-9a-fA-F]{3,}\b"
        r"|(?:PC|RA|MEPC|MTVAL|EXCVADDR|call)\s*[:=]\s*(?:0x)?4[0-9a-fA-F]{7}"
        r"|CRASH DETECTED ON PREVIOUS BOOT"
    ),
    PLATFORM_ESP8266: (
        r"0x[0-9a-fA-F]{3,}\b"
        r"|\b(?![0-9]{8}\b)[0-9a-fA-F]{8}\b"
        r"|(?:PC|EXCVADDR|call)\s*[:=]\s*(?:0x)?4[0-9a-fA-F]{7}"
        r"|[eE]xception \(\d+\):"
        r"|>>>stack>>>"
        r"|CRASH DETECTED ON PREVIOUS BOOT"
    ),
    PLATFORM_RP2: r"0x[0-9a-fA-F]{3,}\b|CRASH DETECTED ON PREVIOUS BOOT",
    PLATFORM_NRF52: r"0x[0-9a-fA-F]{3,}\b|Last crash:",
}

PLATFORM_HOOKS: Final[dict[str, frozenset[str]]] = {
    "show_logs": frozenset({PLATFORM_NRF52}),
    "upload_program": frozenset({PLATFORM_NRF52}),
    "process_stacktrace": frozenset(STACKTRACE_GATES),
}


# The registry only speaks for in-tree platforms; a target platform
# supplied via external_components is normally not in Platform and falls
# back to probing the imported package, as the CLI did before the
# registry. Deliberate trade: an external component that shadows an
# in-tree platform name (the meta finder allows it) is treated as the
# in-tree platform here, so its own hooks are not probed.
_IN_TREE_PLATFORMS: Final = frozenset(Platform)


def has_registered_hook(platform: str, hook: str) -> bool:
    """True when *platform* declares *hook* in ``PLATFORM_HOOKS``.

    Callers that defer imports key off this: a registered hook is known
    to exist, so ``get_platform_hook`` can wait until it is needed;
    anything else must be probed up front so availability is reported
    at session start. Keeping the predicate here keeps the resolution
    rule in one module.
    """
    return platform in PLATFORM_HOOKS[hook]


def get_platform_hook(platform: str, hook: str) -> Callable[..., Any] | None:
    """Return ``esphome.components.<platform>.<hook>`` or None.

    In-tree platforms not registered for the hook return None without
    being imported. A registered platform that no longer defines the
    hook also returns None, so a stale registry degrades to the generic
    path instead of raising.
    """
    registered = has_registered_hook(platform, hook)
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


def get_stacktrace_handler(platform: str) -> Callable[..., Any] | None:
    """Resolve ``process_stacktrace`` for *platform*, degrading with a log.

    Stacktrace decoding is a diagnostic nicety. This only distinguishes
    an import failure from an ordinary capability gap so the message is
    accurate; it returns None for both, and callers own any further
    containment. Shared so the user-facing message lives in one place.
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
