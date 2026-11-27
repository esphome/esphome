"""``esphome prune``."""

from __future__ import annotations

import argparse
import logging

_LOGGER = logging.getLogger(__name__)


def _recommended_idf_versions() -> set[str]:
    """The ESP-IDF versions the recommended frameworks build with."""
    from esphome.components.esp32 import (
        ARDUINO_FRAMEWORK_VERSION_LOOKUP,
        ARDUINO_IDF_VERSION_LOOKUP,
        ESP_IDF_FRAMEWORK_VERSION_LOOKUP,
    )

    arduino = ARDUINO_FRAMEWORK_VERSION_LOOKUP["recommended"]
    return {
        str(ESP_IDF_FRAMEWORK_VERSION_LOOKUP["recommended"]),
        str(ARDUINO_IDF_VERSION_LOOKUP[arduino]),
    }


def command_prune(args: argparse.Namespace) -> int | None:
    from esphome.espidf.prune import prune_idf_installs

    try:
        removed = prune_idf_installs(_recommended_idf_versions(), dry_run=args.dry_run)
    except OSError as err:
        _LOGGER.error("Error pruning ESP-IDF installs: %s", err)
        return 1

    if not removed:
        _LOGGER.info("Nothing to prune")
    elif not args.dry_run:
        _LOGGER.info("Done!")
    return 0
