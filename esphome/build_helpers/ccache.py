"""Shared ccache policy for build backends: env-knob parsing, binary
resolution, and default ``CCACHE_*`` values."""

from __future__ import annotations

import logging
import os
from pathlib import Path

from esphome.framework_helpers import strip_win_long_path_prefix, tool_version_runs
from esphome.helpers import FALSY_ENV_STRINGS, TRUTHY_ENV_STRINGS

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
    if not lowered:
        # ENV KNOB= (Docker/CI) has always read as a disable
        return False
    if lowered in TRUTHY_ENV_STRINGS:
        return True
    if lowered in FALSY_ENV_STRINGS:
        return False
    _LOGGER.warning("Ignoring unrecognized %s=%r; use 1 or 0", name, raw)
    return None


def resolve_ccache_path() -> str | None:
    """The ccache binary to wrap compiles with, or None when disabled.

    An explicit ``ESPHOME_CCACHE_ENABLE=1`` skips the runnability probe; the
    Windows extended-length prefix is stripped before probing (#18399).
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

    # An unset build_path means the env was built before preload; fail loudly
    # rather than silently drop CCACHE_BASEDIR.
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
