"""Removing installs from the machine-global tools cache."""

from __future__ import annotations

import logging
from pathlib import Path

from esphome.framework_helpers import rename_with_retry
from esphome.helpers import rmtree

_LOGGER = logging.getLogger(__name__)

PRUNING_SUFFIX = ".pruning"


def subdirs(path: Path, symlinks: bool = False) -> list[Path]:
    """The directories in ``path``; a symlink is the user's own, so off by default."""
    try:
        return sorted(
            entry
            for entry in path.iterdir()
            if entry.is_dir() and (symlinks or not entry.is_symlink())
        )
    except (FileNotFoundError, NotADirectoryError):
        return []


def is_leftover(path: Path) -> bool:
    return path.name.endswith(PRUNING_SUFFIX)


def remove_trees(paths: list[Path], dry_run: bool) -> None:
    """Delete each tree, or only say so on a dry run."""
    # Leftovers of an interrupted prune go first so no rename can collide
    for path in sorted(paths, key=lambda path: not is_leftover(path)):
        if dry_run:
            _LOGGER.info("Would delete %s", path)
            continue
        _LOGGER.info("Deleting %s", path)
        # Renamed first: an interrupted delete must not leave a partial tree
        # that still passes for an install
        if not is_leftover(path):
            doomed = path.with_name(path.name + PRUNING_SUFFIX)
            rename_with_retry(path, doomed)
            path = doomed
        rmtree(path)
