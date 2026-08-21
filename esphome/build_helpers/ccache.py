"""Shared ccache policy for build backends.

One place for the ``CCACHE_*`` defaults (every backend) and for the probe
and enable rules (backends that call ``resolve_ccache_path``: PlatformIO
and the native Arduino build). The ESP-IDF backend keeps its own
``IDF_CCACHE_ENABLE`` gate and does not probe.
"""

from __future__ import annotations

import logging
import os
from pathlib import Path
import subprocess

from esphome.framework_helpers import strip_win_long_path_prefix

_LOGGER = logging.getLogger(__name__)


def _ccache_runs(ccache: str) -> bool:
    """Return True when the ``ccache`` found on PATH actually runs.

    ``shutil.which`` proves existence, not runnability: on Windows it also
    matches ``.bat``/``.cmd`` wrappers and stale package-manager shims whose
    target is gone. Wrapping compiles around such a find fails every compile
    step with an opaque OS error, so probe once and fall back to compiling
    without ccache when the probe fails.
    """
    try:
        subprocess.run(
            [ccache, "--version"],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=15,
            # Repo-wide convention (posix_spawn fast path); see the
            # close_fds=False call sites across esphome/ and script/helpers.py
            close_fds=False,
        )
    except (OSError, subprocess.SubprocessError):
        _LOGGER.warning(
            "Ignoring ccache at %s because it failed to run; compiling without ccache",
            ccache,
        )
        return False
    return True


def resolve_ccache_path() -> str | None:
    """The ccache binary to wrap compiles with, or None when disabled.

    Shared policy for every backend: on by default when a runnable ccache is
    on PATH, ``ESPHOME_CCACHE_ENABLE=0`` opts out, and an explicit ``=1``
    warns when no binary is found and skips the runnability probe. The
    Windows extended-length prefix is stripped before probing so the probe
    validates the exact string the build will execute (#18399).
    """
    import shutil

    from esphome.helpers import get_bool_env

    explicit = "ESPHOME_CCACHE_ENABLE" in os.environ
    if explicit and not get_bool_env("ESPHOME_CCACHE_ENABLE"):
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
