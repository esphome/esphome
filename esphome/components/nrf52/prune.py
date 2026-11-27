"""Remove the nRF Connect SDK installs that ESPHome no longer builds with by default."""

from __future__ import annotations

from collections.abc import Collection
from pathlib import Path

from esphome.build_helpers.prune import installed_name, remove_guarded, subdirs

from .framework import _TOOLCHAIN_VERSIONS, get_sdk_nrf_tools_path

# The directories one install lock guards, by the lock's name prefix
_LOCKED_INSTALLS = {"sdk": ("frameworks", "penvs"), "toolchain": ("toolchains",)}


def prune_sdk_nrf_installs(
    keep_versions: Collection[str], dry_run: bool = False
) -> list[Path]:
    """Remove every SDK version except ``keep_versions`` (names like ``v2.9.2``).

    Their Python environments go with them, and so does every toolchain that
    this ESPHome version does not build with. Returns the removed directories
    (the ones that would be removed on a dry run).
    """
    root = get_sdk_nrf_tools_path()
    # A symlinked SDK is the user's own checkout; its environment stays with it
    symlinked = {
        framework.name
        for framework in subdirs(root / "frameworks", symlinks=True)
        if framework.is_symlink()
    }
    keep = {"sdk": {*keep_versions, *symlinked}, "toolchain": _TOOLCHAIN_VERSIONS}
    # (lock prefix, version) -> what to remove of that install
    doomed: dict[tuple[str, str], list[Path]] = {}
    for prefix, kinds in _LOCKED_INSTALLS.items():
        for kind in kinds:
            for install in subdirs(root / kind):
                if install.name not in keep[prefix]:
                    # What is left of an interrupted prune belongs to the
                    # install it was renamed from
                    version = installed_name(install)
                    doomed.setdefault((prefix, version), []).append(install)

    removed: list[Path] = []
    for (prefix, version), paths in doomed.items():
        guarded = [root / kind / version for kind in _LOCKED_INSTALLS[prefix]]
        lock = root / f"{prefix}-{version}.lock"
        removed += remove_guarded(lock, guarded, paths, dry_run)
    return removed
