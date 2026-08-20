"""Warn about platformio options a native build backend drops."""

from __future__ import annotations

import logging

from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)


def warn_ignored_platformio_options(consumed: frozenset[str], toolchain: str) -> None:
    """Warn for component-added platformio options the native build drops.

    YAML ``esphome: platformio_options:`` keys are warned about during code
    generation and never reach ``CORE.platformio_options`` under a native
    toolchain, so anything left here came from ``cg.add_platformio_option()``
    calls (e.g. an external component) and would be silently ignored.
    ``consumed`` names the keys the backend honors.
    """
    for key in sorted(CORE.platformio_options or {}):
        if key not in consumed:
            _LOGGER.warning(
                "platformio_options->%s is ignored when building with the "
                "native '%s' toolchain",
                key,
                toolchain,
            )
