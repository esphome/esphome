"""Remove the nRF Connect SDK installs that ESPHome no longer builds with by default."""

from __future__ import annotations

from collections.abc import Collection
from contextlib import suppress
from pathlib import Path

from esphome.build_helpers.prune import PRUNING_SUFFIX, remove_trees, subdirs

from .framework import _TOOLCHAIN_VERSIONS, get_sdk_nrf_tools_path


def prune_sdk_nrf_installs(
    keep_versions: Collection[str], dry_run: bool = False
) -> list[Path]:
    """Remove every SDK version except ``keep_versions`` (names like ``v2.9.2``).

    Their Python environments go with them, and so does every toolchain that
    this ESPHome version does not build with. Returns the removed directories
    (the ones that would be removed on a dry run).
    """
    root = get_sdk_nrf_tools_path()
    removed = [
        install
        for kind in ("frameworks", "penvs")
        for install in subdirs(root / kind)
        if install.name not in keep_versions
    ]
    removed += [
        toolchain
        for toolchain in subdirs(root / "toolchains")
        if toolchain.name not in _TOOLCHAIN_VERSIONS
    ]
    remove_trees(removed, dry_run)

    if not dry_run:
        for path in removed:
            name = path.name.removesuffix(PRUNING_SUFFIX)
            if path.parent.name == "toolchains":
                lock, installs = f"toolchain-{name}.lock", ("toolchains",)
            else:
                lock, installs = f"sdk-{name}.lock", ("frameworks", "penvs")
            if not any((root / kind / name).exists() for kind in installs):
                with suppress(OSError):
                    (root / lock).unlink()

    return removed
