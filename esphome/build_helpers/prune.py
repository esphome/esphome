"""Removing installs from the machine-global tools cache."""

from __future__ import annotations

from collections.abc import Collection
from contextlib import suppress
import logging
from pathlib import Path

from esphome.framework_helpers import rename_with_retry
from esphome.helpers import rmtree

_LOGGER = logging.getLogger(__name__)

# What is being removed is renamed first, so an interrupted delete cannot
# leave a partial tree that still passes for an install
PRUNING_SUFFIX = ".pruning"
# PlatformIO takes every directory in its platform and package folders for an
# install, whatever its name, except names that start with "_tmp_installing".
# The extra ".pruning-" keeps these apart from its own temporary directories.
PLATFORMIO_PRUNING_PREFIX = "_tmp_installing.pruning-"


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
    """Whether an interrupted prune left ``path`` behind."""
    return path.name.endswith(PRUNING_SUFFIX) or path.name.startswith(
        PLATFORMIO_PRUNING_PREFIX
    )


def installed_name(path: Path) -> str:
    """The name ``path`` has, or had before a prune renamed it."""
    return path.name.removesuffix(PRUNING_SUFFIX).removeprefix(
        PLATFORMIO_PRUNING_PREFIX
    )


def _renamed(path: Path, platformio: bool) -> Path:
    if platformio:
        return path.with_name(PLATFORMIO_PRUNING_PREFIX + path.name)
    return path.with_name(path.name + PRUNING_SUFFIX)


def with_leftover(path: Path, platformio: bool = False) -> list[Path]:
    """``path`` and what an interrupted prune left of it, whichever exist."""
    return [
        candidate
        for candidate in (path, _renamed(path, platformio))
        if candidate.exists() and not candidate.is_symlink()
    ]


def remove_trees(paths: list[Path], dry_run: bool, platformio: bool = False) -> None:
    """Delete each tree or file, or only say so on a dry run.

    ``platformio`` renames with the prefix PlatformIO ignores.
    """
    # Leftovers of an interrupted prune go first so no rename can collide
    for path in sorted(paths, key=lambda path: not is_leftover(path)):
        if dry_run:
            _LOGGER.info("Would delete %s", path)
            continue
        _LOGGER.info("Deleting %s", path)
        if not is_leftover(path):
            doomed = _renamed(path, platformio)
            rename_with_retry(path, doomed)
            path = doomed
        if path.is_dir() and not path.is_symlink():
            rmtree(path)
        else:
            path.unlink(missing_ok=True)


def remove_guarded(
    lock_path: Path, guarded: Collection[Path], paths: list[Path], dry_run: bool
) -> list[Path]:
    """Remove installs under the install lock that guards them.

    ``guarded`` are the installs the lock is for and ``paths`` what to remove
    of them. Nothing is removed while a build holds the lock, which it does
    for as long as it installs. The lock file goes too once none of the
    installs is left. Returns what was removed (``paths`` on a dry run).
    """
    if dry_run:
        remove_trees(paths, dry_run)
        return paths
    from filelock import FileLock, Timeout

    lock = FileLock(str(lock_path), fallback_to_soft=False)
    try:
        lock.acquire(timeout=0)
    except Timeout:
        _LOGGER.warning(
            "Keeping %s, a build is installing it",
            ", ".join(str(path) for path in paths),
        )
        return []
    try:
        remove_trees(paths, dry_run)
        if not any(path.exists() for path in guarded):
            with suppress(OSError):
                lock_path.unlink()
    finally:
        lock.release()
    return paths
