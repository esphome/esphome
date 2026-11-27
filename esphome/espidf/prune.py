"""Remove the ESP-IDF installs that ESPHome no longer builds with by default."""

from __future__ import annotations

from collections.abc import Collection
from contextlib import suppress
import logging
from pathlib import Path

from esphome.espidf.framework import _get_idf_tool_paths, get_idf_tools_path
from esphome.framework_helpers import rename_with_retry
from esphome.helpers import rmtree

_LOGGER = logging.getLogger(__name__)

_PRUNING_SUFFIX = ".pruning"


def _subdirs(path: Path) -> list[Path]:
    try:
        return sorted(entry for entry in path.iterdir() if entry.is_dir())
    except (FileNotFoundError, NotADirectoryError):
        return []


def _used_tools(frameworks: list[Path]) -> set[Path] | None:
    """Tool version directories the frameworks build with, None if unknown."""
    tools_path = get_idf_tools_path() / "tools"
    env = {"IDF_TOOLS_PATH": str(get_idf_tools_path()), "IDF_PATH": ""}
    used: set[Path] = set()
    for framework in frameworks:
        try:
            paths_to_export, export_vars = _get_idf_tool_paths(framework, env)
        except RuntimeError as err:
            _LOGGER.warning(
                "Keeping all ESP-IDF tools, could not tell which ones %s uses: %s",
                framework,
                err,
            )
            return None
        found: set[Path] = set()
        for value in (*paths_to_export, *export_vars.values()):
            with suppress(ValueError):
                parts = Path(value).relative_to(tools_path).parts
                if len(parts) >= 2:
                    found.add(tools_path / parts[0] / parts[1])
        if not found:
            _LOGGER.warning(
                "Keeping all ESP-IDF tools, %s uses none of the installed ones",
                framework,
            )
            return None
        used |= found
    return used


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
    keep_versions: Collection[str], dry_run: bool = False
) -> list[Path]:
    """Remove every ESP-IDF version except ``keep_versions``.

    Python environments and tool versions that the kept versions do not use
    go with them. Returns the removed directories (the ones that would be
    removed on a dry run).
    """
    root = get_idf_tools_path()

    kept: list[Path] = []
    removed: list[Path] = []
    for framework in _subdirs(root / "frameworks"):
        # A symlink is the user's own checkout
        if framework.is_symlink() or framework.name in keep_versions:
            kept.append(framework)
        else:
            removed.append(framework)

    kept_versions = {framework.name for framework in kept}
    removed += [
        penv
        for penv in _subdirs(root / "penvs")
        if not penv.is_symlink() and penv.name not in kept_versions
    ]

    tools: list[Path] = []
    if (used := _used_tools(kept)) is not None:
        tools = [tool for tool in _subdirs(root / "tools") if not tool.is_symlink()]
        removed += [
            version
            for tool in tools
            for version in _subdirs(tool)
            if not version.is_symlink() and version not in used
        ]

    # Leftovers of an interrupted prune go first so no rename can collide
    removed.sort(key=lambda path: not path.name.endswith(_PRUNING_SUFFIX))
    for path in removed:
        _remove(path, dry_run)

    if not dry_run:
        for tool in tools:
            with suppress(OSError):
                tool.rmdir()

    return removed
