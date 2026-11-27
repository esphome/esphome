"""``esphome prune``."""

from __future__ import annotations

import argparse
import logging

from esphome.core import EsphomeError

_LOGGER = logging.getLogger(__name__)


def command_prune(args: argparse.Namespace) -> int | None:
    from esphome.espidf.prune import prune_idf_installs

    if args.days < 1:
        raise EsphomeError("--days must be at least 1")

    try:
        removed = prune_idf_installs(args.days, dry_run=args.dry_run)
    except OSError as err:
        _LOGGER.error("Error pruning ESP-IDF installs: %s", err)
        return 1

    if not removed:
        _LOGGER.info("Nothing to prune")
    elif not args.dry_run:
        _LOGGER.info("Done!")
    return 0
