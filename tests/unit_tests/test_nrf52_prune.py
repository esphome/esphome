"""Tests for esphome.components.nrf52.prune."""

from __future__ import annotations

import logging
from pathlib import Path

from filelock import FileLock
import pytest

from esphome.components.nrf52.framework import TOOLCHAIN_VERSION
from esphome.components.nrf52.prune import prune_sdk_nrf_installs

_KEEP = "v2.9.2"
_OLD = "v2.6.1"
_OLD_TOOLCHAIN = "0.16.8"


@pytest.fixture
def root(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    root = tmp_path / "sdk-nrf"
    monkeypatch.setenv("ESPHOME_SDK_NRF_PREFIX", str(root))
    return root


def _sdk(root: Path, version: str) -> Path:
    (root / "penvs" / version / "bin").mkdir(parents=True)
    (root / "frameworks" / version / "zephyr").mkdir(parents=True)
    (root / f"sdk-{version}.lock").touch()
    return root / "frameworks" / version


def _toolchain(root: Path, version: str) -> Path:
    toolchain = root / "toolchains" / version
    (toolchain / "arm-zephyr-eabi").mkdir(parents=True)
    (root / f"toolchain-{version}.lock").touch()
    return toolchain


def test_prune_keeps_the_listed_sdk_and_the_current_toolchains(root: Path) -> None:
    old = _sdk(root, _OLD)
    sdk = _sdk(root, _KEEP)
    old_toolchain = _toolchain(root, _OLD_TOOLCHAIN)
    toolchain = _toolchain(root, TOOLCHAIN_VERSION)
    (root / "ccache").mkdir()

    removed = prune_sdk_nrf_installs({_KEEP})

    assert set(removed) == {old, root / "penvs" / _OLD, old_toolchain}
    assert sorted(path.name for path in root.iterdir()) == [
        "ccache",
        "frameworks",
        "penvs",
        f"sdk-{_KEEP}.lock",
        f"toolchain-{TOOLCHAIN_VERSION}.lock",
        "toolchains",
    ]
    assert [path.name for path in (root / "frameworks").iterdir()] == [_KEEP]
    assert [path.name for path in (root / "penvs").iterdir()] == [_KEEP]
    assert [path.name for path in (root / "toolchains").iterdir()] == [
        TOOLCHAIN_VERSION
    ]
    assert sdk.is_dir()
    assert toolchain.is_dir()


def test_prune_dry_run_changes_nothing(
    root: Path, caplog: pytest.LogCaptureFixture
) -> None:
    old = _sdk(root, _OLD)

    with caplog.at_level(logging.INFO):
        removed = prune_sdk_nrf_installs({_KEEP}, dry_run=True)

    assert removed == [old, root / "penvs" / _OLD]
    assert old.is_dir()
    assert (root / f"sdk-{_OLD}.lock").is_file()
    assert f"Would delete {old}" in caplog.text


def test_prune_finishes_an_interrupted_prune(root: Path) -> None:
    """A leftover goes, and its lock only when the install is gone too."""
    sdk = _sdk(root, _KEEP)
    reinstalled = root / "frameworks" / f"{_KEEP}.pruning"
    reinstalled.mkdir()
    (root / f"sdk-{_OLD}.lock").touch()
    leftover = root / "penvs" / f"{_OLD}.pruning"
    leftover.mkdir()

    assert set(prune_sdk_nrf_installs({_KEEP})) == {reinstalled, leftover}
    assert not leftover.exists()
    assert not (root / f"sdk-{_OLD}.lock").exists()
    assert sdk.is_dir()


def test_prune_keeps_an_sdk_a_build_is_installing(
    root: Path, caplog: pytest.LogCaptureFixture
) -> None:
    old = _sdk(root, _OLD)
    old_toolchain = _toolchain(root, _OLD_TOOLCHAIN)

    with FileLock(str(root / f"sdk-{_OLD}.lock")):
        assert prune_sdk_nrf_installs({_KEEP}) == [old_toolchain]
        assert (root / f"sdk-{_OLD}.lock").is_file()

    assert old.is_dir()
    assert (root / "penvs" / _OLD).is_dir()
    assert f"Keeping {old}, {root / 'penvs' / _OLD}, a build is installing it" in (
        caplog.text
    )

    # The build is done: the next prune removes it
    assert prune_sdk_nrf_installs({_KEEP}) == [old, root / "penvs" / _OLD]
    assert not (root / f"sdk-{_OLD}.lock").exists()


def test_prune_keeps_the_environment_of_a_symlinked_sdk(root: Path) -> None:
    checkout = root / "my-sdk-nrf"
    checkout.mkdir(parents=True)
    link = root / "frameworks" / "v3.0.0"
    link.parent.mkdir()
    try:
        link.symlink_to(checkout, target_is_directory=True)
    except OSError:
        pytest.skip("symlinks not available")
    penv = root / "penvs" / "v3.0.0"
    penv.mkdir(parents=True)

    assert prune_sdk_nrf_installs({_KEEP}) == []
    assert link.is_symlink()
    assert penv.is_dir()


def test_prune_without_an_install_does_nothing(root: Path) -> None:
    assert prune_sdk_nrf_installs({_KEEP}) == []
    assert not root.exists()
