"""Tests for esphome.espidf.prune."""

from __future__ import annotations

import json
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


def _install(
    root: Path,
    version: str,
    tools: dict[str, str],
    *,
    days_ago: float | None = 0,
) -> Path:
    """Lay out an install of ``version`` whose tools.json lists ``tools``.

    ``days_ago`` ages the last-used marker; None leaves the marker out.
    """
    framework = root / "frameworks" / version
    (framework / "tools").mkdir(parents=True)
    (framework / "tools" / "tools.json").write_text(
        json.dumps(
            {
                "tools": [
                    {"name": name, "versions": [{"name": tool_version}]}
                    for name, tool_version in tools.items()
                ]
            }
        ),
        encoding="utf-8",
    )
    if days_ago is not None:
        marker = framework / ESPHOME_LAST_USED_FILE
        marker.touch()
        used = time.time() - days_ago * _DAY
        os.utime(marker, (used, used))
    (root / "penvs" / version).mkdir(parents=True)
    for name, tool_version in tools.items():
        (root / "tools" / name / tool_version / "bin").mkdir(
            parents=True, exist_ok=True
        )
    return framework


def test_prune_removes_old_version_and_what_only_it_needed(idf_root: Path) -> None:
    old = _install(idf_root, "5.5.6", {"gcc": "14.2.0", "ninja": "1.12.1"}, days_ago=40)
    new = _install(idf_root, "6.1.1", {"gcc": "15.2.0", "ninja": "1.12.1"})

    removed = prune_idf_installs(set(), 30)

    assert set(removed) == {
        old,
        idf_root / "penvs" / "5.5.6",
        idf_root / "tools" / "gcc" / "14.2.0",
    }
    assert not old.exists()
    assert not (idf_root / "penvs" / "5.5.6").exists()
    assert not (idf_root / "tools" / "gcc" / "14.2.0").exists()
    assert not list(idf_root.rglob("*.pruning"))
    assert new.is_dir()
    assert (idf_root / "penvs" / "6.1.1").is_dir()
    assert (idf_root / "tools" / "gcc" / "15.2.0").is_dir()
    assert (idf_root / "tools" / "ninja" / "1.12.1").is_dir()


def test_prune_keeps_recently_used_version(idf_root: Path) -> None:
    recent = _install(idf_root, "5.5.6", {"gcc": "14.2.0"}, days_ago=29)

    assert prune_idf_installs(set(), 30) == []
    assert recent.is_dir()
    assert (idf_root / "tools" / "gcc" / "14.2.0").is_dir()


def test_prune_keeps_listed_version_regardless_of_age(idf_root: Path) -> None:
    default = _install(idf_root, "6.1.1", {"gcc": "15.2.0"}, days_ago=400)

    assert prune_idf_installs({"6.1.1"}, 0) == []
    assert default.is_dir()


def test_prune_zero_days_removes_everything_not_listed(idf_root: Path) -> None:
    used_now = _install(idf_root, "5.5.6", {"gcc": "14.2.0"})
    untracked = _install(idf_root, "6.0.1", {"gcc": "15.2.0"}, days_ago=None)
    default = _install(idf_root, "6.1.1", {"gcc": "15.2.0"})

    prune_idf_installs({"6.1.1"}, 0)

    assert not used_now.exists()
    assert not untracked.exists()
    assert default.is_dir()
    assert not (idf_root / "tools" / "gcc" / "14.2.0").exists()
    assert (idf_root / "tools" / "gcc" / "15.2.0").is_dir()


def test_prune_starts_the_clock_for_an_untracked_install(idf_root: Path) -> None:
    """An install made before last-use tracking is kept and stamped."""
    untracked = _install(idf_root, "5.5.6", {"gcc": "14.2.0"}, days_ago=None)

    assert prune_idf_installs(set(), 30) == []
    assert (untracked / ESPHOME_LAST_USED_FILE).is_file()


def test_prune_dry_run_changes_nothing(
    idf_root: Path, caplog: pytest.LogCaptureFixture
) -> None:
    old = _install(idf_root, "5.5.6", {"gcc": "14.2.0"}, days_ago=40)
    untracked = _install(idf_root, "6.0.1", {"gcc": "15.2.0"}, days_ago=None)

    with caplog.at_level(logging.INFO):
        removed = prune_idf_installs(set(), 30, dry_run=True)

    assert set(removed) == {
        old,
        idf_root / "penvs" / "5.5.6",
        idf_root / "tools" / "gcc" / "14.2.0",
    }
    assert old.is_dir()
    assert (idf_root / "tools" / "gcc" / "14.2.0").is_dir()
    assert not (untracked / ESPHOME_LAST_USED_FILE).exists()
    assert f"Would delete {old}" in caplog.text


def test_prune_removes_empty_tool_directory(idf_root: Path) -> None:
    _install(idf_root, "5.5.6", {"esp-rom-elfs": "20241011"}, days_ago=40)
    _install(idf_root, "6.1.1", {"gcc": "15.2.0"})

    prune_idf_installs(set(), 30)

    assert not (idf_root / "tools" / "esp-rom-elfs").exists()
    assert (idf_root / "tools" / "gcc").is_dir()


def test_prune_removes_orphans(idf_root: Path) -> None:
    """A venv or tool version that no framework accounts for goes too."""
    _install(idf_root, "6.1.1", {"gcc": "15.2.0"})
    orphan_penv = idf_root / "penvs" / "5.4.2"
    orphan_penv.mkdir()
    orphan_tool = idf_root / "tools" / "gcc" / "13.2.0"
    orphan_tool.mkdir()

    assert set(prune_idf_installs(set(), 30)) == {orphan_penv, orphan_tool}
    assert not orphan_penv.exists()
    assert not orphan_tool.exists()


def test_prune_finishes_an_interrupted_prune(idf_root: Path) -> None:
    """Trees a killed prune left renamed are removed, whatever their age."""
    _install(idf_root, "6.1.1", {"gcc": "15.2.0"})
    old = _install(idf_root, "5.5.6", {"gcc": "14.2.0"}, days_ago=40)
    leftovers = [
        idf_root / "frameworks" / "5.5.6.pruning",
        idf_root / "penvs" / "5.5.6.pruning",
        idf_root / "tools" / "gcc" / "14.2.0.pruning",
    ]
    for leftover in leftovers:
        leftover.mkdir()

    removed = prune_idf_installs(set(), 30)

    assert set(leftovers) <= set(removed)
    assert not old.exists()
    assert not list(idf_root.rglob("*.pruning"))


def test_prune_keeps_tools_when_a_kept_framework_is_unreadable(
    idf_root: Path, caplog: pytest.LogCaptureFixture
) -> None:
    old = _install(idf_root, "5.5.6", {"gcc": "14.2.0"}, days_ago=40)
    torn = _install(idf_root, "6.1.1", {"gcc": "15.2.0"})
    (torn / "tools" / "tools.json").write_text("{", encoding="utf-8")

    removed = prune_idf_installs(set(), 30)

    assert set(removed) == {old, idf_root / "penvs" / "5.5.6"}
    assert (idf_root / "tools" / "gcc" / "14.2.0").is_dir()
    assert "Keeping all ESP-IDF tools" in caplog.text


def test_prune_leaves_symlinked_framework_alone(idf_root: Path) -> None:
    checkout = _install(idf_root, "checkout", {"gcc": "16.1.0"}, days_ago=400)
    target = idf_root / "my-esp-idf"
    checkout.rename(target)
    try:
        checkout.symlink_to(target, target_is_directory=True)
    except OSError:
        pytest.skip("symlinks not available")

    assert prune_idf_installs(set(), 0) == []
    assert checkout.is_symlink()
    assert (idf_root / "tools" / "gcc" / "16.1.0").is_dir()


def test_prune_without_an_install_does_nothing(idf_root: Path) -> None:
    assert prune_idf_installs(set(), 30) == []
    assert not idf_root.exists()
