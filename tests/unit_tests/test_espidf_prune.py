"""Tests for esphome.espidf.prune."""

from __future__ import annotations

from collections.abc import Generator
import logging
import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.espidf.prune import prune_idf_installs


@pytest.fixture
def idf_root(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    root = tmp_path / "idf"
    monkeypatch.setenv("ESPHOME_ESP_IDF_PREFIX", str(root))
    return root


@pytest.fixture
def tool_exports() -> Generator[dict[str, tuple[list[str], dict[str, str]]]]:
    """What the tool resolution reports, by framework version.

    A version that is not listed fails to resolve.
    """
    exports: dict[str, tuple[list[str], dict[str, str]]] = {}

    def resolve(
        framework: Path, env: dict[str, str]
    ) -> tuple[list[str], dict[str, str]]:
        if framework.name not in exports:
            raise RuntimeError("Missing ESP-IDF tools: gcc")
        return exports[framework.name]

    with patch("esphome.espidf.prune._get_idf_tool_paths", side_effect=resolve):
        yield exports


def _framework(root: Path, version: str) -> Path:
    (root / "penvs" / version).mkdir(parents=True)
    framework = root / "frameworks" / version
    framework.mkdir(parents=True)
    return framework


def _tool(root: Path, name: str, version: str) -> Path:
    tool = root / "tools" / name / version
    (tool / "bin").mkdir(parents=True)
    return tool


def test_prune_keeps_only_the_listed_version_and_its_tools(
    idf_root: Path, tool_exports: dict[str, tuple[list[str], dict[str, str]]]
) -> None:
    old = _framework(idf_root, "5.5.6")
    old_gcc = _tool(idf_root, "gcc", "14.2.0")
    new = _framework(idf_root, "6.1.1")
    new_gcc = _tool(idf_root, "gcc", "15.2.0")
    ninja = _tool(idf_root, "ninja", "1.12.1")
    rom_elfs = _tool(idf_root, "esp-rom-elfs", "20260528")
    tool_exports["6.1.1"] = (
        [str(new_gcc / "bin"), str(ninja), "/usr/bin", str(new_gcc.parent)],
        {"ESP_ROM_ELF_DIR": f"{rom_elfs}{os.sep}", "IDF_X": "1"},
    )

    removed = prune_idf_installs({"6.1.1"})

    assert set(removed) == {old, idf_root / "penvs" / "5.5.6", old_gcc}
    assert not old.exists()
    assert not (idf_root / "penvs" / "5.5.6").exists()
    assert not old_gcc.exists()
    assert not list(idf_root.rglob("*.pruning"))
    assert new.is_dir()
    assert (idf_root / "penvs" / "6.1.1").is_dir()
    assert new_gcc.is_dir()
    assert ninja.is_dir()
    assert rom_elfs.is_dir()


def test_prune_removes_everything_when_the_listed_version_is_not_installed(
    idf_root: Path, tool_exports: dict[str, tuple[list[str], dict[str, str]]]
) -> None:
    old = _framework(idf_root, "5.5.6")
    _tool(idf_root, "gcc", "14.2.0")

    prune_idf_installs({"6.1.1"})

    assert not old.exists()
    assert not (idf_root / "penvs" / "5.5.6").exists()
    assert not (idf_root / "tools" / "gcc").exists()


@pytest.mark.parametrize("exports", [None, (["/usr/bin"], {"IDF_X": "1"})])
def test_prune_keeps_tools_it_cannot_account_for(
    exports: tuple[list[str], dict[str, str]] | None,
    idf_root: Path,
    tool_exports: dict[str, tuple[list[str], dict[str, str]]],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A kept version that fails to resolve, or resolves to nothing installed."""
    old = _framework(idf_root, "5.5.6")
    old_gcc = _tool(idf_root, "gcc", "14.2.0")
    _framework(idf_root, "6.1.1")
    if exports is not None:
        tool_exports["6.1.1"] = exports

    removed = prune_idf_installs({"6.1.1"})

    assert set(removed) == {old, idf_root / "penvs" / "5.5.6"}
    assert old_gcc.is_dir()
    assert "Keeping all ESP-IDF tools" in caplog.text


def test_prune_removes_tool_leftovers_it_cannot_account_for(
    idf_root: Path, tool_exports: dict[str, tuple[list[str], dict[str, str]]]
) -> None:
    """What an interrupted prune left needs no lookup to be removed."""
    _framework(idf_root, "6.1.1")
    gcc = _tool(idf_root, "gcc", "15.2.0")
    leftover = idf_root / "tools" / "gcc" / "14.2.0.pruning"
    leftover.mkdir()

    assert prune_idf_installs({"6.1.1"}) == [leftover]
    assert gcc.is_dir()


def test_prune_dry_run_changes_nothing(
    idf_root: Path,
    tool_exports: dict[str, tuple[list[str], dict[str, str]]],
    caplog: pytest.LogCaptureFixture,
) -> None:
    old = _framework(idf_root, "5.5.6")
    old_gcc = _tool(idf_root, "gcc", "14.2.0")

    with caplog.at_level(logging.INFO):
        removed = prune_idf_installs({"6.1.1"}, dry_run=True)

    assert set(removed) == {old, idf_root / "penvs" / "5.5.6", old_gcc}
    assert old.is_dir()
    assert (idf_root / "penvs" / "5.5.6").is_dir()
    assert old_gcc.is_dir()
    assert f"Would delete {old}" in caplog.text


def test_prune_finishes_an_interrupted_prune(
    idf_root: Path, tool_exports: dict[str, tuple[list[str], dict[str, str]]]
) -> None:
    """Trees a killed prune left renamed are removed with the rest."""
    _framework(idf_root, "6.1.1")
    new_gcc = _tool(idf_root, "gcc", "15.2.0")
    tool_exports["6.1.1"] = ([str(new_gcc / "bin")], {})
    old = _framework(idf_root, "5.5.6")
    leftovers = [
        idf_root / "frameworks" / "5.5.6.pruning",
        idf_root / "penvs" / "5.5.6.pruning",
        idf_root / "tools" / "gcc" / "14.2.0.pruning",
    ]
    for leftover in leftovers:
        leftover.mkdir()

    removed = prune_idf_installs({"6.1.1"})

    assert set(leftovers) <= set(removed)
    assert not old.exists()
    assert not list(idf_root.rglob("*.pruning"))
    assert new_gcc.is_dir()


def test_prune_leaves_symlinks_alone(
    idf_root: Path, tool_exports: dict[str, tuple[list[str], dict[str, str]]]
) -> None:
    """A symlinked framework is kept, and so are the tools it uses."""
    checkout = idf_root / "my-esp-idf"
    toolchain = idf_root / "my-gcc"
    checkout.mkdir(parents=True)
    toolchain.mkdir()
    gcc = _tool(idf_root, "gcc", "16.1.0")
    tool_exports["checkout"] = ([str(gcc / "bin")], {})
    link = idf_root / "frameworks" / "checkout"
    tool_link = idf_root / "tools" / "gcc" / "custom"
    link.parent.mkdir()
    (idf_root / "penvs" / "checkout").mkdir(parents=True)
    try:
        link.symlink_to(checkout, target_is_directory=True)
        tool_link.symlink_to(toolchain, target_is_directory=True)
    except OSError:
        pytest.skip("symlinks not available")

    assert prune_idf_installs({"6.1.1"}) == []
    assert link.is_symlink()
    assert tool_link.is_symlink()
    assert (idf_root / "penvs" / "checkout").is_dir()
    assert gcc.is_dir()


def test_prune_without_an_install_does_nothing(
    idf_root: Path, tool_exports: dict[str, tuple[list[str], dict[str, str]]]
) -> None:
    assert prune_idf_installs({"6.1.1"}) == []
    assert not idf_root.exists()
