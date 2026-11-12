"""Tests for the Zephyr framework installer."""

from collections.abc import Iterator
import os
from pathlib import Path
import time
from typing import Any
from unittest.mock import MagicMock, patch

import platformdirs
import pytest

from esphome.components.zephyr import framework, sdk_install
from esphome.core import EsphomeError

TOOLCHAIN: str = "riscv64-zephyr-elf"


def test_check_and_install_builds_the_venv(tmp_path: Path) -> None:
    """Each Zephyr version gets its own venv, built from zephyr/requirements.txt
    under that version's install lock."""
    with (
        patch.object(framework, "get_zephyr_tools_path", return_value=tmp_path),
        patch.object(framework, "ensure_venv") as ensure,
        patch("filelock.FileLock") as file_lock,
    ):
        framework.check_and_install("v4.5.0")

    ensure.assert_called_once_with(
        tmp_path / "penvs" / "v4.5.0",
        Path(framework.__file__).parent / "requirements.txt",
        "v4.5.0",
    )
    assert framework._REQUIREMENTS.is_file()
    assert [Path(c.args[0]).name for c in file_lock.call_args_list] == [
        "penv-v4.5.0.lock"
    ]


@pytest.fixture
def zephyr_tools(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    """Keep the test out of the real ~/.cache/esphome."""
    monkeypatch.setenv("ESPHOME_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    return (tmp_path / "cache").resolve()


def _make_framework(tmp_path: Path, sdk_version: str = "1.0.1") -> Path:
    framework_path = tmp_path / "framework"
    (framework_path / "zephyr").mkdir(parents=True)
    (framework_path / "zephyr" / "SDK_VERSION").write_text(sdk_version)
    return framework_path


@pytest.fixture
def mock_download(zephyr_tools: Path) -> Iterator[MagicMock]:
    """download_and_extract() as seen by the installer; it creates the extract dir."""

    def _extract(*args: Any, **kwargs: Any) -> str:
        Path(args[3]).mkdir(parents=True, exist_ok=True)
        return "https://example.com/archive"

    with (
        patch.object(sdk_install, "download_and_extract", side_effect=_extract) as m,
        patch.object(
            sdk_install,
            "get_sdk_host_info",
            return_value=("linux", "x86_64", "tar.xz"),
        ),
    ):
        yield m


def test_toolchain_path_is_per_sdk_version_and_toolchain(zephyr_tools: Path) -> None:
    assert (
        framework._get_toolchain_path("1.0.1", TOOLCHAIN)
        == zephyr_tools / "toolchains" / f"zephyr-sdk-1.0.1-{TOOLCHAIN}"
    )


def test_toolchain_path_default_is_global_cache(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv("ESPHOME_ZEPHYR_PREFIX", raising=False)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False))
        / "zephyr"
        / "toolchains"
        / f"zephyr-sdk-1.0.1-{TOOLCHAIN}"
    ).resolve()
    assert framework._get_toolchain_path("1.0.1", TOOLCHAIN) == expected


def test_install_toolchain_downloads_minimal_and_toolchain(
    tmp_path: Path, mock_download: MagicMock
) -> None:
    """The SDK version comes from the workspace's zephyr/SDK_VERSION."""
    framework_path = _make_framework(tmp_path)
    sdk_path = framework._get_toolchain_path("1.0.1", TOOLCHAIN)

    assert framework.install_toolchain(framework_path, TOOLCHAIN) == sdk_path

    (minimal, toolchain) = mock_download.call_args_list
    assert minimal.args[0] == sdk_install.SDK_NG_MINIMAL_MIRRORS
    assert minimal.args[3] == sdk_path
    assert toolchain.args[0] == sdk_install.SDK_NG_TOOLCHAIN_GNU_MIRRORS
    assert toolchain.args[1]["VERSION"] == "1.0.1"
    assert toolchain.args[1]["toolchain"] == TOOLCHAIN
    assert toolchain.args[3] == sdk_path / "gnu" / TOOLCHAIN
    assert (sdk_path / ".ready").exists()


def test_install_toolchain_locks_per_sdk_and_toolchain(
    tmp_path: Path, mock_download: MagicMock
) -> None:
    framework_path = _make_framework(tmp_path)

    with patch("filelock.FileLock") as file_lock:
        framework.install_toolchain(framework_path, TOOLCHAIN)

    lock_files = [Path(c.args[0]).name for c in file_lock.call_args_list]
    assert lock_files == [f"toolchain-zephyr-sdk-1.0.1-{TOOLCHAIN}.lock"]


def test_install_toolchain_raises_without_sdk_version(
    tmp_path: Path, zephyr_tools: Path
) -> None:
    """A workspace with no SDK_VERSION file names the file it could not read."""
    with pytest.raises(EsphomeError, match="SDK_VERSION"):
        framework.install_toolchain(tmp_path, TOOLCHAIN)


def test_install_toolchain_raises_on_an_empty_sdk_version(
    tmp_path: Path, zephyr_tools: Path
) -> None:
    framework_path = _make_framework(tmp_path, sdk_version="\n")

    with pytest.raises(EsphomeError, match="names no Zephyr SDK version"):
        framework.install_toolchain(framework_path, TOOLCHAIN)


def test_install_toolchain_removes_only_stale_partial_downloads(
    tmp_path: Path, mock_download: MagicMock
) -> None:
    """A fresh leftover may belong to a running build."""
    framework_path = _make_framework(tmp_path)
    toolchains = framework._get_toolchain_path("1.0.1", TOOLCHAIN).parent
    toolchains.mkdir(parents=True)
    stale = toolchains / "zephyr-sdk-0.17.4-arm-zephyr-eabi.toolchain.archive.part"
    fresh = toolchains / "zephyr-sdk-1.0.2-arm-zephyr-eabi.toolchain.archive.part"
    for leftover in (stale, fresh):
        leftover.write_text("")
    two_days_ago = time.time() - 2 * 24 * 60 * 60
    os.utime(stale, (two_days_ago, two_days_ago))

    framework.install_toolchain(framework_path, TOOLCHAIN)

    assert not stale.exists()
    assert fresh.exists()
