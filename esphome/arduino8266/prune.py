"""Remove the Arduino ESP8266 installs that ESPHome no longer builds with."""

from __future__ import annotations

from collections.abc import Collection
from contextlib import suppress
from pathlib import Path

from esphome.arduino8266.framework import TOOLCHAIN_VERSION, get_arduino8266_tools_path
from esphome.build_helpers.prune import PRUNING_SUFFIX, remove_trees, subdirs


def prune_arduino8266_installs(
    keep_frameworks: Collection[str], dry_run: bool = False
) -> list[Path]:
    """Remove every Arduino core except ``keep_frameworks`` (release tags).

    Every toolchain other than the one this ESPHome version builds with goes
    too. Returns the removed directories (the ones that would be removed on a
    dry run).
    """
    root = get_arduino8266_tools_path()
    removed = [
        framework
        for framework in subdirs(root / "frameworks")
        if framework.name not in keep_frameworks
    ]
    removed += [
        toolchain
        for toolchain in subdirs(root / "toolchains")
        if toolchain.name != TOOLCHAIN_VERSION
    ]
    remove_trees(removed, dry_run)

    if not dry_run:
        for path in removed:
            install = path.with_name(path.name.removesuffix(PRUNING_SUFFIX))
            if not install.exists():
                with suppress(OSError):
                    install.with_name(f"{install.name}.lock").unlink()

    return removed
