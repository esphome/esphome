"""Shared ccache policy for build backends.

``ccache_defaults_env`` serves the backends that export ``CCACHE_*`` into a
build subprocess (native ESP-IDF and Arduino); ``resolve_ccache_path``
carries the probe and enable rules (PlatformIO and the native Arduino
build). The ESP-IDF backend keeps ``IDF_CCACHE_ENABLE`` as a
higher-precedence override and falls back to the shared resolver (probe
included) when it is unset; PlatformIO feeds its SCons wrapper script
through env channels instead of ``CCACHE_*`` defaults.
"""

from __future__ import annotations

import logging
import os
from pathlib import Path

from esphome.framework_helpers import strip_win_long_path_prefix, tool_version_runs

_LOGGER = logging.getLogger(__name__)


def _ccache_runs(ccache: str) -> bool:
    """Return True when the ``ccache`` found on PATH actually runs."""
    return tool_version_runs(
        ccache,
        "Ignoring ccache at %s because it failed to run; compiling without ccache",
    )


def parse_enable_env(name: str) -> bool | None:
    """Strictly parse an on/off environment knob; None when unset or invalid.

    ``bool(str)`` truthiness would flip ``no``/``off`` to enabled, so only
    1/true/yes/on and 0/false/no/off count; anything else warns and reads
    as unset so the caller's default policy applies.
    """
    raw = os.environ.get(name)
    if raw is None:
        return None
    lowered = raw.strip().lower()
    if lowered in ("1", "true", "yes", "on"):
        return True
    if lowered in ("0", "false", "no", "off"):
        return False
    _LOGGER.warning("Ignoring unrecognized %s=%r; use 1 or 0", name, raw)
    return None


def resolve_ccache_path() -> str | None:
    """The ccache binary to wrap compiles with, or None when disabled.

    Shared policy for every backend: on by default when a runnable ccache is
    on PATH, ``ESPHOME_CCACHE_ENABLE=0`` opts out, and an explicit ``=1``
    warns when no binary is found and skips the runnability probe;
    any other value warns and is treated as unset. The
    Windows extended-length prefix is stripped before probing so the probe
    validates the exact string the build will execute (#18399).
    """
    import shutil

    explicit = parse_enable_env("ESPHOME_CCACHE_ENABLE")
    if explicit is False:
        return None
    ccache = shutil.which("ccache")
    if ccache is None:
        if explicit:
            _LOGGER.warning(
                "ESPHOME_CCACHE_ENABLE is set but no ccache binary is on PATH; "
                "compiling without ccache"
            )
        return None
    ccache = strip_win_long_path_prefix(ccache)
    if not explicit and not _ccache_runs(ccache):
        return None
    return ccache


def ccache_defaults_env(cache_dir: Path) -> dict[str, str]:
    """Default ``CCACHE_*`` values for a build subprocess (not os.environ).

    Values the user already set in the environment are respected. Depend
    mode is on: both native backends emit depfiles (-MMD / CMake), which
    keeps cache-miss overhead low.
    """
    from esphome.core import CORE

    # build_path is set during preload for every config-loading command; unset
    # means the caller built the environment too early. Fail loudly rather
    # than silently drop CCACHE_BASEDIR (losing cross-device cache hits).
    if CORE.build_path is None:
        raise ValueError(
            "CORE.build_path must be set before constructing the build environment"
        )
    defaults = {
        "CCACHE_DIR": str(cache_dir),
        "CCACHE_NOHASHDIR": "true",
        "CCACHE_DEPEND": "1",
        "CCACHE_BASEDIR": str(Path(CORE.build_path).resolve()),
    }
    return {k: v for k, v in defaults.items() if k not in os.environ}
