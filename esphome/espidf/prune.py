"""Remove the ESP-IDF installs that ESPHome no longer builds with by default."""

from __future__ import annotations

from collections.abc import Collection
from contextlib import suppress
import logging
from pathlib import Path

from esphome.build_helpers.prune import is_leftover, remove_trees, subdirs
from esphome.espidf.framework import _get_idf_tool_paths, get_idf_tools_path

_LOGGER = logging.getLogger(__name__)


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
    for framework in subdirs(root / "frameworks", symlinks=True):
        if framework.is_symlink() or framework.name in keep_versions:
            kept.append(framework)
        else:
            removed.append(framework)

    kept_versions = {framework.name for framework in kept}
    removed += [
        penv for penv in subdirs(root / "penvs") if penv.name not in kept_versions
    ]

    used = _used_tools(kept)
    tools = subdirs(root / "tools")
    removed += [
        version
        for tool in tools
        for version in subdirs(tool)
        if is_leftover(version) or (used is not None and version not in used)
    ]

    remove_trees(removed, dry_run)

    if not dry_run:
        for tool in tools:
            with suppress(OSError):
                tool.rmdir()

    return removed
