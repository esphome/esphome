"""Remove ESP-IDF installs that have not been used recently."""

from __future__ import annotations

from collections.abc import Collection
from contextlib import suppress
import json
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
        return sorted(entry for entry in path.iterdir() if entry.is_dir())
    except (FileNotFoundError, NotADirectoryError):
        return []


def _last_used(framework: Path, now: float, dry_run: bool) -> float:
    marker = framework / ESPHOME_LAST_USED_FILE
    with suppress(FileNotFoundError):
        return marker.stat().st_mtime
    # Installed before last-use tracking existed: the clock starts now
    if not dry_run:
        with suppress(OSError):
            marker.touch()
    return now


def _referenced_tools(frameworks: list[Path]) -> set[tuple[str, str]] | None:
    """Tool versions the frameworks install, or None if one cannot be read."""
    referenced: set[tuple[str, str]] = set()
    for framework in frameworks:
        tools_json = framework / "tools" / "tools.json"
        try:
            tools = json.loads(tools_json.read_text(encoding="utf-8"))["tools"]
            referenced.update(
                (tool["name"], version["name"])
                for tool in tools
                for version in tool["versions"]
            )
        except (OSError, ValueError, KeyError, TypeError) as err:
            _LOGGER.warning(
                "Keeping all ESP-IDF tools, could not read %s: %s", tools_json, err
            )
            return None
    return referenced


def _remove(path: Path, dry_run: bool) -> None:
    if dry_run:
        _LOGGER.info("Would delete %s", path)
        return
    _LOGGER.info("Deleting %s", path)
    # Renamed first: an interrupted delete must not leave a partial tree
    # that still passes for an install
    if not path.name.endswith(_PRUNING_SUFFIX):
        doomed = path.with_name(path.name + _PRUNING_SUFFIX)
        rename_with_retry(path, doomed)
        path = doomed
    rmtree(path)


def prune_idf_installs(
    keep_versions: Collection[str], max_age_days: float, dry_run: bool = False
) -> list[Path]:
    """Remove ESP-IDF versions unused for ``max_age_days``.

    Versions in ``keep_versions`` stay regardless of age. Python environments
    and tool versions that no remaining framework needs go with them. Returns
    the removed directories (the ones that would be removed on a dry run).
    """
    root = get_idf_tools_path()
    now = time.time()
    max_age = max_age_days * _SECONDS_PER_DAY

    kept: list[Path] = []
    removed: list[Path] = []
    for framework in _subdirs(root / "frameworks"):
        if framework.name.endswith(_PRUNING_SUFFIX):
            removed.append(framework)
        elif (
            framework.is_symlink()
            or framework.name in keep_versions
            or max(now - _last_used(framework, now, dry_run), 0.0) < max_age
        ):
            kept.append(framework)
        else:
            removed.append(framework)

    kept_versions = {framework.name for framework in kept}
    removed += [
        penv
        for penv in _subdirs(root / "penvs")
        if not penv.is_symlink() and penv.name not in kept_versions
    ]

    tool_dirs: list[Path] = []
    if (referenced := _referenced_tools(kept)) is not None:
        tool_dirs = [tool for tool in _subdirs(root / "tools") if not tool.is_symlink()]
        removed += [
            version
            for tool in tool_dirs
            for version in _subdirs(tool)
            if not version.is_symlink() and (tool.name, version.name) not in referenced
        ]

    # Leftovers of an interrupted prune go first so no rename can collide
    removed.sort(key=lambda path: not path.name.endswith(_PRUNING_SUFFIX))
    for path in removed:
        _remove(path, dry_run)

    if not dry_run:
        for tool in tool_dirs:
            with suppress(OSError):
                tool.rmdir()

    return removed
