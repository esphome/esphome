"""``esphome prune``."""

from __future__ import annotations

import argparse
import logging

from esphome.core import EsphomeError

_LOGGER = logging.getLogger(__name__)


def _default_idf_versions() -> set[str]:
    """The ESP-IDF versions this ESPHome release builds with by default."""
    from esphome.components.esp32 import (
        ARDUINO_FRAMEWORK_VERSION_LOOKUP,
        ARDUINO_IDF_VERSION_LOOKUP,
        ESP_IDF_FRAMEWORK_VERSION_LOOKUP,
    )

    versions = set(ESP_IDF_FRAMEWORK_VERSION_LOOKUP.values())
    versions |= {
        ARDUINO_IDF_VERSION_LOOKUP[arduino]
        for arduino in ARDUINO_FRAMEWORK_VERSION_LOOKUP.values()
        if arduino in ARDUINO_IDF_VERSION_LOOKUP
    }
    return {str(version) for version in versions}


def command_prune(args: argparse.Namespace) -> int | None:
    from esphome.espidf.prune import prune_idf_installs

    if args.days < 0:
        raise EsphomeError("--days must not be negative")

    try:
        removed = prune_idf_installs(
            _default_idf_versions(), args.days, dry_run=args.dry_run
        )
    except OSError as err:
        _LOGGER.error("Error pruning ESP-IDF installs: %s", err)
        return 1

    if not removed:
        _LOGGER.info("Nothing to prune")
    elif not args.dry_run:
        _LOGGER.info("Done!")
    return 0
