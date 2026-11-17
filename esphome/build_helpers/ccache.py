"""Shared ccache policy for build backends: env-knob parsing, binary
resolution, and default ``CCACHE_*`` values."""

from __future__ import annotations

import functools
import logging
import os
from pathlib import Path
import re

from esphome.framework_helpers import (
    strip_win_long_path_prefix,
    tool_version_probe,
    tool_version_runs,
)
from esphome.helpers import FALSY_ENV_STRINGS, TRUTHY_ENV_STRINGS

_LOGGER = logging.getLogger(__name__)


# Before 4.7 ccache stores a pch consumer whose header is newer than one
# second without that header, so a later build reuses it after the header
# changed (#21621)
CCACHE_PCH_MIN_VERSION = (4, 7)

_VERSION_RE = re.compile(r"ccache version (\d+(?:\.\d+)*)")


@functools.cache
def _ccache_probe(ccache: str) -> str | Exception:
    """``ccache --version``, run once per binary per process."""
    return tool_version_probe(ccache)


def _ccache_runs(ccache: str) -> bool:
    """Return True when the ``ccache`` found on PATH actually runs."""
    return tool_version_runs(
        ccache,
        "Ignoring ccache at %s because it failed to run; compiling without ccache",
        probe=_ccache_probe,
    )


@functools.cache
def _ccache_too_old_for_pch(ccache: str) -> bool:
    output = _ccache_probe(ccache)
    if not isinstance(output, str) or (found := _VERSION_RE.search(output)) is None:
        return False
    if tuple(map(int, found[1].split("."))) >= CCACHE_PCH_MIN_VERSION:
        return False
    _LOGGER.info(
        "ccache %s at %s can reuse stale objects with a precompiled header; "
        "compiling without it (upgrade ccache to %s or newer, or set "
        "ESPHOME_PCH_ENABLE=1 to force)",
        found[1],
        ccache,
        ".".join(map(str, CCACHE_PCH_MIN_VERSION)),
    )
    return True


def ccache_breaks_pch() -> bool:
    """Whether the ccache this build uses is too old for a precompiled header.
    ESP-IDF builds check the ccache IDF finds in CMake instead (build_gen.espidf)."""
    return (ccache := resolve_ccache_path()) is not None and _ccache_too_old_for_pch(
        ccache
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


def resolve_absolute_ccache_path() -> str | None:
    """``resolve_ccache_path`` for the ninja backends, which run their
    commands from the build directory, where a relative path is lost."""
    from esphome.build_helpers.ninja import absolute_tool

    ccache = resolve_ccache_path()
    return absolute_tool(ccache) if ccache else None


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
        # ccache expands $VAR in its settings; $$ is a literal $
        "CCACHE_DIR": str(cache_dir).replace("$", "$$"),
        "CCACHE_NOHASHDIR": "true",
        "CCACHE_DEPEND": "1",
        "CCACHE_BASEDIR": str(Path(CORE.build_path).resolve()).replace("$", "$$"),
    }
    return {k: v for k, v in defaults.items() if k not in os.environ}


def effective_ccache_basedir() -> str:
    """The prefix ccache strips from hashed paths: a usable user
    CCACHE_BASEDIR, else the resolved build path."""
    from esphome.core import CORE

    raw = os.environ.get("CCACHE_BASEDIR")
    if raw is not None and Path(raw).is_absolute() and len(Path(raw).parts) > 1:
        return raw
    return str(Path(CORE.build_path).resolve())


def ccache_env(ccache: str | None, tools_cache: tuple[str, str]) -> dict[str, str]:
    """The ccache settings for a build subprocess (not os.environ).

    ``ccache`` is the pre-resolved binary (resolve_ccache_path), or None when
    disabled; ``tools_cache`` is the backend's tools cache spec, which holds
    its ccache dir. The pch settings include ``time_macros``: a cached
    object can keep an older ``__DATE__`` or ``__TIME__``.
    """
    if ccache is None:
        return {}
    from esphome.build_helpers.pch import ccache_pch_env
    from esphome.build_helpers.tools_cache import tools_cache_path

    env = ccache_defaults_env(tools_cache_path(*tools_cache) / "ccache")
    env.update(ccache_pch_env())
    return env
