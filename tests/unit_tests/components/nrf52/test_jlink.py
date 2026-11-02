"""Finding SEGGER's J-Link software for 'esphome upload --device JLINK'."""

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.nrf52 import jlink
from esphome.components.nrf52.jlink import find_jlink_commander


def _install(
    directory: Path, exe: str = "JLinkExe", library: str | None = "libjlinkarm.so"
) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    commander = directory / exe
    commander.write_text("")
    if library is not None:
        (directory / library).write_text("")
    return commander


@pytest.fixture
def linux(tmp_path: Path):
    with (
        patch.object(jlink.sys, "platform", "linux"),
        patch.dict(jlink._JLINK_INSTALL_DIRS, {"linux": (tmp_path / "opt",)}),
        patch.object(jlink.shutil, "which", return_value=None) as which,
    ):
        yield which


def test_found_on_path(linux, tmp_path: Path) -> None:
    commander = _install(tmp_path / "bin")
    linux.return_value = str(commander)
    assert find_jlink_commander() == commander


def test_path_link_to_install_dir(linux, tmp_path: Path) -> None:
    real = _install(tmp_path / "opt" / "JLink_V812")
    link = tmp_path / "bin" / "JLinkExe"
    link.parent.mkdir()
    link.symlink_to(real)
    linux.return_value = str(link)
    assert find_jlink_commander() == link


@pytest.mark.usefixtures("linux")
def test_found_in_install_dir(tmp_path: Path) -> None:
    commander = _install(tmp_path / "opt" / "JLink")
    assert find_jlink_commander() == commander


@pytest.mark.usefixtures("linux")
def test_newest_versioned_install(tmp_path: Path) -> None:
    _install(tmp_path / "opt" / "JLink_V794")
    newest = _install(tmp_path / "opt" / "JLink_V812")
    assert find_jlink_commander() == newest


@pytest.mark.usefixtures("linux")
def test_newest_version_compares_numbers(tmp_path: Path) -> None:
    _install(tmp_path / "opt" / "JLink_V812")
    _install(tmp_path / "opt" / "JLink_V924")
    newest = _install(tmp_path / "opt" / "JLink_V1000")
    assert find_jlink_commander() == newest


@pytest.mark.usefixtures("linux")
def test_newest_version_compares_suffix(tmp_path: Path) -> None:
    _install(tmp_path / "opt" / "JLink_V924")
    newest = _install(tmp_path / "opt" / "JLink_V924a")
    assert find_jlink_commander() == newest


@pytest.mark.usefixtures("linux")
def test_commander_without_library_is_skipped(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    commander = _install(tmp_path / "opt" / "JLink", library=None)
    assert find_jlink_commander() is None
    assert f"Skipping {commander}" in caplog.text


@pytest.mark.usefixtures("linux")
def test_only_versioned_library_is_skipped(tmp_path: Path) -> None:
    """The runner loads libjlinkarm.so itself, not a versioned name."""
    _install(tmp_path / "opt" / "JLink", library="libjlinkarm.so.8")
    assert find_jlink_commander() is None


@pytest.mark.usefixtures("linux")
def test_not_installed() -> None:
    assert find_jlink_commander() is None


def test_windows_ignores_path(tmp_path: Path) -> None:
    """The JDK's jlink.exe on PATH must not be taken for SEGGER's JLink.exe."""
    registry = _install(tmp_path / "reg", exe="JLink.exe", library="JLink_x64.dll")
    with (
        patch.object(jlink.sys, "platform", "win32"),
        patch.dict(jlink._JLINK_INSTALL_DIRS, {"win32": (tmp_path / "pf",)}),
        patch.object(jlink, "_jlink_from_windows_registry", return_value=registry),
        patch.object(jlink.shutil, "which") as which,
    ):
        assert find_jlink_commander() == registry
    which.assert_not_called()
