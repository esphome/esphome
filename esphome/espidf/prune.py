"""Remove ESP-IDF installs that have not been used recently."""

from __future__ import annotations

from contextlib import suppress
import logging
from pathlib import Path
import time

from esphome.espidf.framework import ESPHOME_LAST_USED_FILE, get_idf_tools_path
from esphome.framework_helpers import rename_with_retry
from esphome.helpers import rmtree

_LOGGER = logging.getLogger(__name__)

_PRUNING_SUFFIX = ".pruning"
_SECONDS_PER_DAY = 86400


def _subdirs(path: Path) -> list[Path]:
    try:
        return sorted(
            entry
            for entry in path.iterdir()
            if entry.is_dir() and not entry.is_symlink()
        )
    except (FileNotFoundError, NotADirectoryError):
        return []


def _is_leftover(path: Path) -> bool:
    return path.name.endswith(_PRUNING_SUFFIX)


def _unused(path: Path, cutoff: float, dry_run: bool) -> bool:
    if _is_leftover(path):
        return True
    marker = path / ESPHOME_LAST_USED_FILE
    with suppress(FileNotFoundError):
        return marker.stat().st_mtime < cutoff
    # Installed before last-use tracking existed: the clock starts now
    if not dry_run:
        with suppress(OSError):
            marker.touch()
    return False


def _remove(path: Path, dry_run: bool) -> None:
    if dry_run:
        _LOGGER.info("Would delete %s", path)
        return
    _LOGGER.info("Deleting %s", path)
    # Renamed first: an interrupted delete must not leave a partial tree
    # that still passes for an install
    if not _is_leftover(path):
        doomed = path.with_name(path.name + _PRUNING_SUFFIX)
        rename_with_retry(path, doomed)
        path = doomed
    rmtree(path)


def prune_idf_installs(max_age_days: float, dry_run: bool = False) -> list[Path]:
    """Remove ESP-IDF versions and tool versions unused for ``max_age_days``.

    A Python environment goes with its framework. Returns the removed
    directories (the ones that would be removed on a dry run).
    """
    root = get_idf_tools_path()
    cutoff = time.time() - max_age_days * _SECONDS_PER_DAY

    frameworks = _subdirs(root / "frameworks")
    removed = [path for path in frameworks if _unused(path, cutoff, dry_run)]
    # A symlinked framework is not listed, so its environment is left alone
    gone = {path.name for path in removed}
    removed += [
        penv
        for penv in _subdirs(root / "penvs")
        if _is_leftover(penv)
        or penv.name in gone
        or not (root / "frameworks" / penv.name).is_dir()
    ]

    tools = _subdirs(root / "tools")
    removed += [
        version
        for tool in tools
        for version in _subdirs(tool)
        if _unused(version, cutoff, dry_run)
    ]

    # Leftovers of an interrupted prune go first so no rename can collide
    removed.sort(key=lambda path: not _is_leftover(path))
    for path in removed:
        _remove(path, dry_run)

    if not dry_run:
        for tool in tools:
            with suppress(OSError):
                tool.rmdir()

    return removed
