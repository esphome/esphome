"""Tests for esphome.arduino8266.prune."""

from __future__ import annotations

import logging
from pathlib import Path

from filelock import FileLock
import pytest

from esphome.arduino8266.framework import TOOLCHAIN_VERSION
from esphome.arduino8266.prune import prune_arduino8266_installs

_KEEP = "3.1.2-esphome.4"


@pytest.fixture
def root(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    root = tmp_path / "arduino8266"
    monkeypatch.setenv("ESPHOME_ARDUINO8266_PREFIX", str(root))
    return root


def _install(root: Path, kind: str, tag: str) -> Path:
    install = root / kind / tag
    (install / "bin").mkdir(parents=True)
    install.with_name(f"{tag}.lock").touch()
    return install


def test_prune_keeps_the_listed_core_and_the_current_toolchain(root: Path) -> None:
    old_core = _install(root, "frameworks", "3.1.2-esphome.3")
    core = _install(root, "frameworks", _KEEP)
    old_toolchain = _install(root, "toolchains", "10.3.0-esphome.0")
    toolchain = _install(root, "toolchains", TOOLCHAIN_VERSION)

    removed = prune_arduino8266_installs({_KEEP})

    assert set(removed) == {old_core, old_toolchain}
    assert sorted(path.name for path in (root / "frameworks").iterdir()) == [
        _KEEP,
        f"{_KEEP}.lock",
    ]
    assert sorted(path.name for path in (root / "toolchains").iterdir()) == [
        TOOLCHAIN_VERSION,
        f"{TOOLCHAIN_VERSION}.lock",
    ]
    assert core.is_dir()
    assert toolchain.is_dir()


def test_prune_dry_run_changes_nothing(
    root: Path, caplog: pytest.LogCaptureFixture
) -> None:
    old_core = _install(root, "frameworks", "3.1.2-esphome.3")

    with caplog.at_level(logging.INFO):
        assert prune_arduino8266_installs({_KEEP}, dry_run=True) == [old_core]

    assert old_core.is_dir()
    assert old_core.with_name("3.1.2-esphome.3.lock").is_file()
    assert f"Would delete {old_core}" in caplog.text


def test_prune_finishes_an_interrupted_prune(root: Path) -> None:
    """A leftover goes, and its lock only when the install is gone too."""
    core = _install(root, "frameworks", _KEEP)
    reinstalled = root / "frameworks" / f"{_KEEP}.pruning"
    reinstalled.mkdir()
    old_lock = root / "frameworks" / "3.1.2-esphome.3.lock"
    old_lock.touch()
    leftover = root / "frameworks" / "3.1.2-esphome.3.pruning"
    leftover.mkdir()

    assert prune_arduino8266_installs({_KEEP}) == [leftover, reinstalled]
    assert not leftover.exists()
    assert not old_lock.exists()
    assert core.is_dir()


def test_prune_removes_a_core_reinstalled_after_an_interrupted_prune(
    root: Path,
) -> None:
    old_core = _install(root, "frameworks", "3.1.2-esphome.3")
    leftover = root / "frameworks" / "3.1.2-esphome.3.pruning"
    (leftover / "bin").mkdir(parents=True)

    assert prune_arduino8266_installs({_KEEP}) == [old_core, leftover]
    assert not list((root / "frameworks").iterdir())


def test_prune_keeps_a_core_a_build_is_installing(root: Path) -> None:
    old_core = _install(root, "frameworks", "3.1.2-esphome.3")
    lock = old_core.with_name("3.1.2-esphome.3.lock")

    with FileLock(str(lock)):
        assert prune_arduino8266_installs({_KEEP}) == []
        assert lock.is_file()

    assert old_core.is_dir()


def test_prune_leaves_symlinks_alone(root: Path) -> None:
    checkout = root / "my-core"
    checkout.mkdir(parents=True)
    link = root / "frameworks" / "checkout"
    link.parent.mkdir()
    try:
        link.symlink_to(checkout, target_is_directory=True)
    except OSError:
        pytest.skip("symlinks not available")

    assert prune_arduino8266_installs({_KEEP}) == []
    assert link.is_symlink()


def test_prune_without_an_install_does_nothing(root: Path) -> None:
    assert prune_arduino8266_installs({_KEEP}) == []
    assert not root.exists()
