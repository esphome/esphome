"""Tests for esphome.espidf.prune."""

from __future__ import annotations

import logging
import os
from pathlib import Path
import time

import pytest

from esphome.espidf.framework import ESPHOME_LAST_USED_FILE
from esphome.espidf.prune import prune_idf_installs

_DAY = 86400


@pytest.fixture
def idf_root(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    root = tmp_path / "idf"
    monkeypatch.setenv("ESPHOME_ESP_IDF_PREFIX", str(root))
    return root


def _mark(path: Path, days_ago: float | None) -> Path:
    """Create ``path`` last used ``days_ago``; None leaves it unmarked."""
    path.mkdir(parents=True, exist_ok=True)
    if days_ago is not None:
        marker = path / ESPHOME_LAST_USED_FILE
        marker.touch()
        used = time.time() - days_ago * _DAY
        os.utime(marker, (used, used))
    return path


def _framework(root: Path, version: str, days_ago: float | None) -> Path:
    (root / "penvs" / version).mkdir(parents=True)
    return _mark(root / "frameworks" / version, days_ago)


def _tool(root: Path, name: str, version: str, days_ago: float | None) -> Path:
    return _mark(root / "tools" / name / version, days_ago)


def test_prune_removes_what_was_not_used_recently(idf_root: Path) -> None:
    old = _framework(idf_root, "5.5.6", 40)
    old_gcc = _tool(idf_root, "gcc", "14.2.0", 40)
    new = _framework(idf_root, "6.1.1", 1)
    new_gcc = _tool(idf_root, "gcc", "15.2.0", 1)
    # Shared by both versions, so the newer one keeps it fresh
    ninja = _tool(idf_root, "ninja", "1.12.1", 1)

    removed = prune_idf_installs(30)

    assert set(removed) == {old, idf_root / "penvs" / "5.5.6", old_gcc}
    assert not old.exists()
    assert not (idf_root / "penvs" / "5.5.6").exists()
    assert not old_gcc.exists()
    assert not list(idf_root.rglob("*.pruning"))
    assert new.is_dir()
    assert (idf_root / "penvs" / "6.1.1").is_dir()
    assert new_gcc.is_dir()
    assert ninja.is_dir()


def test_prune_keeps_what_is_inside_the_age_limit(idf_root: Path) -> None:
    framework = _framework(idf_root, "5.5.6", 29)
    gcc = _tool(idf_root, "gcc", "14.2.0", 29)

    assert prune_idf_installs(30) == []
    assert prune_idf_installs(28) == [framework, idf_root / "penvs" / "5.5.6", gcc]


def test_prune_starts_the_clock_for_an_untracked_install(idf_root: Path) -> None:
    """An install made before last-use tracking is kept and stamped."""
    framework = _framework(idf_root, "5.5.6", None)
    gcc = _tool(idf_root, "gcc", "14.2.0", None)

    assert prune_idf_installs(30) == []
    assert (framework / ESPHOME_LAST_USED_FILE).is_file()
    assert (gcc / ESPHOME_LAST_USED_FILE).is_file()


def test_prune_dry_run_changes_nothing(
    idf_root: Path, caplog: pytest.LogCaptureFixture
) -> None:
    old = _framework(idf_root, "5.5.6", 40)
    old_gcc = _tool(idf_root, "gcc", "14.2.0", 40)
    untracked = _framework(idf_root, "6.0.1", None)

    with caplog.at_level(logging.INFO):
        removed = prune_idf_installs(30, dry_run=True)

    assert set(removed) == {old, idf_root / "penvs" / "5.5.6", old_gcc}
    assert old.is_dir()
    assert (idf_root / "penvs" / "5.5.6").is_dir()
    assert old_gcc.is_dir()
    assert not (untracked / ESPHOME_LAST_USED_FILE).exists()
    assert f"Would delete {old}" in caplog.text


def test_prune_removes_empty_tool_directory(idf_root: Path) -> None:
    _tool(idf_root, "esp-rom-elfs", "20241011", 40)
    _tool(idf_root, "gcc", "14.2.0", 40)
    _tool(idf_root, "gcc", "15.2.0", 1)

    prune_idf_installs(30)

    assert not (idf_root / "tools" / "esp-rom-elfs").exists()
    assert [path.name for path in (idf_root / "tools" / "gcc").iterdir()] == ["15.2.0"]


def test_prune_removes_environment_without_a_framework(idf_root: Path) -> None:
    _framework(idf_root, "6.1.1", 1)
    orphan = idf_root / "penvs" / "5.4.2"
    orphan.mkdir()

    assert prune_idf_installs(30) == [orphan]
    assert not orphan.exists()


def test_prune_finishes_an_interrupted_prune(idf_root: Path) -> None:
    """Trees a killed prune left renamed are removed, whatever their age."""
    _framework(idf_root, "6.1.1", 1)
    old = _framework(idf_root, "5.5.6", 40)
    leftovers = [
        idf_root / "frameworks" / "5.5.6.pruning",
        idf_root / "penvs" / "5.5.6.pruning",
        idf_root / "tools" / "gcc" / "14.2.0.pruning",
    ]
    for leftover in leftovers:
        _mark(leftover, 0)

    removed = prune_idf_installs(30)

    assert set(leftovers) <= set(removed)
    assert not old.exists()
    assert not list(idf_root.rglob("*.pruning"))


def test_prune_leaves_symlinks_alone(idf_root: Path) -> None:
    checkout = _mark(idf_root / "my-esp-idf", 400)
    toolchain = _mark(idf_root / "my-gcc", 400)
    link = idf_root / "frameworks" / "checkout"
    tool_link = idf_root / "tools" / "gcc" / "custom"
    for path in (link, tool_link, idf_root / "penvs" / "checkout"):
        path.parent.mkdir(parents=True, exist_ok=True)
    (idf_root / "penvs" / "checkout").mkdir()
    try:
        link.symlink_to(checkout, target_is_directory=True)
        tool_link.symlink_to(toolchain, target_is_directory=True)
    except OSError:
        pytest.skip("symlinks not available")

    assert prune_idf_installs(30) == []
    assert link.is_symlink()
    assert tool_link.is_symlink()
    assert (idf_root / "penvs" / "checkout").is_dir()


def test_prune_without_an_install_does_nothing(idf_root: Path) -> None:
    assert prune_idf_installs(30) == []
    assert not idf_root.exists()
