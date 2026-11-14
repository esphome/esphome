"""Tests for the Zephyr framework installer."""

from collections.abc import Iterator
import os
from pathlib import Path
import time
from typing import Any
from unittest.mock import MagicMock, patch

import platformdirs
import pytest

from esphome.components.zephyr import framework, framework_helpers, sdk_install
from esphome.components.zephyr.variants import ZEPHYR_MAINLINE
from esphome.core import EsphomeError

TOOLCHAIN: str = "riscv64-zephyr-elf"


VERSION: str = "v4.5.0"
HALS: frozenset[str] = frozenset({"hal_stm32"})
# The project filter of a workspace with HALS
FILTER: str = ",".join(
    [
        "-hal_.*",
        "-nrf_hw_models",
        *(f"+{h}" for h in sorted(HALS | ZEPHYR_MAINLINE.source.required)),
    ]
)


@pytest.fixture
def zephyr_tools(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    """Keep the test out of the real ~/.cache/esphome."""
    monkeypatch.setenv("ESPHOME_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    return (tmp_path / "cache").resolve()


@pytest.fixture
def workspace(zephyr_tools: Path) -> Path:
    return zephyr_tools / "frameworks" / f"zephyr-{VERSION}"


@pytest.fixture
def mock_west(zephyr_tools: Path, workspace: Path) -> Iterator[MagicMock]:
    """run_command_ok for west and pip, with a built venv; west init leaves a
    workspace."""
    (zephyr_tools / "penvs" / VERSION).mkdir(parents=True)

    def _run(cmd: list[str], **kwargs: Any) -> bool:
        if "init" in cmd:
            _mark_installed(workspace)
            (workspace / ".ready").unlink()
        return True

    with (
        patch.object(framework, "ensure_venv", return_value=False),
        patch.object(framework_helpers, "rmdir"),
        patch.object(framework_helpers, "run_command_ok", side_effect=_run) as run,
    ):
        yield run


def _subcommands(run: MagicMock) -> list[str]:
    return [
        c.args[0][c.args[0].index("west" if "west" in c.args[0] else "pip") + 1]
        for c in run.call_args_list
    ]


def _west_call(run: MagicMock, subcommand: str) -> list[str]:
    (cmd,) = (c.args[0] for c in run.call_args_list if subcommand in c.args[0])
    return cmd[cmd.index(subcommand) :]


def _mark_installed(workspace: Path, hals: frozenset[str] = HALS) -> None:
    (workspace / ".west").mkdir(parents=True, exist_ok=True)
    (workspace / ".west" / "config").touch()
    (workspace / "zephyr" / "scripts").mkdir(parents=True, exist_ok=True)
    (workspace / "zephyr" / "scripts" / "requirements.txt").touch()
    (workspace / ".west_projects").write_text(
        "\n".join(sorted(hals | ZEPHYR_MAINLINE.source.required)), encoding="utf-8"
    )
    (workspace / ".ready").touch()


def _check_and_install(hals: frozenset[str] = HALS) -> None:
    framework.check_and_install(ZEPHYR_MAINLINE, VERSION, hals)


def test_check_and_install_builds_the_venv(
    zephyr_tools: Path, mock_west: MagicMock
) -> None:
    """Each Zephyr version gets its own venv, built from zephyr/requirements.txt
    under that version's lock, inside the workspace's lock."""
    with (
        patch.object(framework, "ensure_venv", return_value=False) as ensure,
        patch("filelock.FileLock") as file_lock,
    ):
        _check_and_install()

    ensure.assert_called_once_with(
        zephyr_tools / "penvs" / VERSION,
        Path(framework.__file__).parent / "requirements.txt",
        VERSION,
    )
    assert framework._REQUIREMENTS.is_file()
    assert [Path(c.args[0]).name for c in file_lock.call_args_list] == [
        f"zephyr-{VERSION}.lock",
        f"penv-{VERSION}.lock",
        f"penv-{VERSION}.lock",
    ]


def test_fresh_install_fetches_only_the_wanted_hals(
    workspace: Path, mock_west: MagicMock
) -> None:
    _check_and_install()

    assert _subcommands(mock_west) == ["init", "config", "update", "list", "install"]
    assert _west_call(mock_west, "init") == [
        "init",
        "-m",
        ZEPHYR_MAINLINE.source.url,
        "-o=--depth=1",
        "--mr",
        VERSION,
        str(workspace),
    ]
    assert _west_call(mock_west, "config")[-1] == FILTER
    assert (workspace / ".ready").exists()
    assert (workspace / ".west_projects").read_text(encoding="utf-8").split() == (
        sorted(HALS | ZEPHYR_MAINLINE.source.required)
    )


def test_installed_workspace_checks_the_hals(
    zephyr_tools: Path, workspace: Path, mock_west: MagicMock
) -> None:
    """Every build checks the names, so an unknown HAL fails on any workspace."""
    _mark_installed(workspace)
    (zephyr_tools / "penvs" / VERSION / f".zephyr_reqs_ready-zephyr-{VERSION}").touch()

    _check_and_install()

    assert _subcommands(mock_west) == ["list"]
    assert set(_west_call(mock_west, "list")) >= HALS


def test_unknown_hal_fails_first(workspace: Path, mock_west: MagicMock) -> None:
    _mark_installed(workspace)
    mock_west.side_effect = None
    mock_west.return_value = False

    with pytest.raises(EsphomeError, match="west list failed .* Zephyr v4.5.0"):
        _check_and_install(frozenset({"hal_unknown"}))

    assert _subcommands(mock_west) == ["list"]


def test_new_hal_fetches_only_that_hal(
    zephyr_tools: Path, workspace: Path, mock_west: MagicMock
) -> None:
    """A workspace only gains HALs; the projects already there are left alone."""
    _mark_installed(workspace, frozenset({"hal_nordic"}))
    (zephyr_tools / "penvs" / VERSION / f".zephyr_reqs_ready-zephyr-{VERSION}").touch()

    _check_and_install()

    assert _subcommands(mock_west) == ["list", "config", "update"]
    assert _west_call(mock_west, "update")[-1:] == ["hal_stm32"]
    assert "+hal_nordic" in _west_call(mock_west, "config")[-1]
    assert "hal_nordic" in (workspace / ".west_projects").read_text(encoding="utf-8")


def test_rebuilt_venv_reinstalls_only_zephyr_requirements(
    zephyr_tools: Path, workspace: Path, mock_west: MagicMock
) -> None:
    _mark_installed(workspace)

    with patch.object(framework, "ensure_venv", return_value=True):
        _check_and_install()

    assert _subcommands(mock_west) == ["list", "install"]
    assert (
        zephyr_tools / "penvs" / VERSION / f".zephyr_reqs_ready-zephyr-{VERSION}"
    ).exists()


def test_installed_workspace_gets_the_patches(
    workspace: Path, mock_west: MagicMock
) -> None:
    _mark_installed(workspace)

    with (
        patch.object(framework, "patch_gen_defines_dts_path") as gen_defines,
        patch.object(framework, "patch_west_build_source_dir_relpath") as west_build,
    ):
        _check_and_install()

    gen_defines.assert_called_once_with(workspace)
    west_build.assert_called_once_with(workspace)


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
    """A fresh leftover may belong to a running build; a resumed part keeps its old meta."""
    framework_path = _make_framework(tmp_path)
    toolchains = framework._get_toolchain_path("1.0.1", TOOLCHAIN).parent
    toolchains.mkdir(parents=True)
    stale = toolchains / "zephyr-sdk-0.17.4-arm-zephyr-eabi.toolchain.archive.part"
    stale_meta = stale.with_name(f"{stale.name}.meta")
    fresh = toolchains / "zephyr-sdk-1.0.2-arm-zephyr-eabi.toolchain.archive.part"
    fresh_meta = fresh.with_name(f"{fresh.name}.meta")
    for leftover in (stale, stale_meta, fresh, fresh_meta):
        leftover.write_text("")
    two_days_ago = time.time() - 2 * 24 * 60 * 60
    for old in (stale, stale_meta, fresh_meta):
        os.utime(old, (two_days_ago, two_days_ago))

    framework.install_toolchain(framework_path, TOOLCHAIN)

    assert not stale.exists()
    assert not stale_meta.exists()
    assert fresh.exists()
    assert fresh_meta.exists()
