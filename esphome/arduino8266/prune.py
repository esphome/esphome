"""Remove the Arduino ESP8266 installs that ESPHome no longer builds with."""

from __future__ import annotations

from collections.abc import Collection
from pathlib import Path

from esphome.arduino8266.framework import TOOLCHAIN_VERSION, get_arduino8266_tools_path
from esphome.build_helpers.prune import installed_name, remove_guarded, subdirs


def prune_arduino8266_installs(
    keep_frameworks: Collection[str], dry_run: bool = False
) -> list[Path]:
    """Remove every Arduino core except ``keep_frameworks`` (release tags).

    Every toolchain other than the one this ESPHome version builds with goes
    too. Returns the removed directories (the ones that would be removed on a
    dry run).
    """
    root = get_arduino8266_tools_path()
    unused = [
        framework
        for framework in subdirs(root / "frameworks")
        if framework.name not in keep_frameworks
    ]
    unused += [
        toolchain
        for toolchain in subdirs(root / "toolchains")
        if toolchain.name != TOOLCHAIN_VERSION
    ]
    # Install -> what to remove of it. What is left of an interrupted prune
    # belongs to the install it was renamed from.
    doomed: dict[Path, list[Path]] = {}
    for path in unused:
        doomed.setdefault(path.with_name(installed_name(path)), []).append(path)
    removed: list[Path] = []
    for install, paths in doomed.items():
        # The installer locks "<install>.lock"
        lock = install.with_name(f"{install.name}.lock")
        removed += remove_guarded(lock, [install], paths, dry_run)
    return removed
