"""Nordic's nrfutil is downloaded into the tools cache and keeps its commands there."""

from __future__ import annotations

import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.nrf52 import framework
from esphome.components.nrf52.framework import (
    _NRFUTIL_LAUNCHERS,
    NRF5SDK_TOOLS_VERSION,
    _nrfutil_target_triple,
    ensure_nordic_nrfutil,
)
from esphome.core import EsphomeError

LINUX = ("linux", "x86_64", "tar.xz")


@pytest.mark.parametrize(
    ("host", "triple"),
    [
        (("macos", "aarch64", "tar.xz"), "aarch64-apple-darwin"),
        (("macos", "x86_64", "tar.xz"), "x86_64-apple-darwin"),
        (LINUX, "x86_64-unknown-linux-gnu"),
        (("windows", "x86_64", "7z"), "x86_64-pc-windows-msvc"),
    ],
)
def test_launcher_triple_follows_the_host(
    host: tuple[str, str, str], triple: str
) -> None:
    with patch.object(framework, "_get_toolchain_platform_info", return_value=host):
        assert _nrfutil_target_triple() == triple


def test_unknown_host_is_a_clear_error() -> None:
    with (
        patch.object(
            framework,
            "_get_toolchain_platform_info",
            return_value=("freebsd", "x86_64", "tar.xz"),
        ),
        pytest.raises(EsphomeError, match="freebsd x86_64"),
    ):
        _nrfutil_target_triple()


# The launcher and its command carry the host's executable suffix
SUFFIX = ".exe" if os.name == "nt" else ""


def _fake_download(url: str, dest: Path, **kwargs: object) -> None:
    dest.write_bytes(b"launcher")


def _fake_install(cmd: list[str], env: dict[str, str]) -> bool:
    """The launcher installs its command under NRFUTIL_HOME."""
    command = Path(env["NRFUTIL_HOME"]) / "bin" / f"nrfutil-nrf5sdk-tools{SUFFIX}"
    command.parent.mkdir(parents=True, exist_ok=True)
    command.write_bytes(b"command")
    return True


def test_first_use_downloads_and_installs_into_the_cache(tmp_path: Path) -> None:
    env: dict[str, str] = {}
    with (
        patch.object(framework, "get_sdk_nrf_tools_path", return_value=tmp_path),
        patch.object(framework, "_get_toolchain_platform_info", return_value=LINUX),
        patch.object(
            framework, "download_with_resume", side_effect=_fake_download
        ) as download,
        patch.object(framework, "run_command_ok", side_effect=_fake_install) as install,
    ):
        exe = ensure_nordic_nrfutil(env)

    # The launcher sits outside the home it installs into, and the command version is pinned
    assert exe == str(tmp_path / "nrfutil" / "launcher" / f"nrfutil{SUFFIX}")
    assert env["NRFUTIL_HOME"] == str(tmp_path / "nrfutil" / "home")
    # The launcher comes from the host's URL and is checked against the pinned digest
    size, sha256 = _NRFUTIL_LAUNCHERS["x86_64-unknown-linux-gnu"]
    assert download.call_args[0][0].endswith(
        f"/x86_64-unknown-linux-gnu/nrfutil{SUFFIX}"
    )
    assert download.call_args[1] == {"sha256": sha256, "size": size}
    assert install.call_args[0][0] == [
        exe,
        "install",
        f"nrf5sdk-tools={NRF5SDK_TOOLS_VERSION}",
    ]
    assert (tmp_path / "nrfutil" / f".ready-{NRF5SDK_TOOLS_VERSION}").exists()


def _installed_cache(tmp_path: Path, with_command: bool = True) -> Path:
    root = tmp_path / "nrfutil"
    (root / "launcher").mkdir(parents=True)
    (root / "launcher" / f"nrfutil{SUFFIX}").write_bytes(b"launcher")
    if with_command:
        (root / "home" / "bin").mkdir(parents=True)
        (root / "home" / "bin" / f"nrfutil-nrf5sdk-tools{SUFFIX}").write_bytes(
            b"command"
        )
    (root / f".ready-{NRF5SDK_TOOLS_VERSION}").touch()
    return root


def test_installed_cache_is_used_without_network(tmp_path: Path) -> None:
    root = _installed_cache(tmp_path)
    env: dict[str, str] = {}
    with (
        patch.object(framework, "get_sdk_nrf_tools_path", return_value=tmp_path),
        patch.object(framework, "download_with_resume") as download,
        patch.object(framework, "run_command_ok") as install,
    ):
        assert ensure_nordic_nrfutil(env) == str(root / "launcher" / f"nrfutil{SUFFIX}")
    download.assert_not_called()
    install.assert_not_called()
    assert env["NRFUTIL_HOME"] == str(root / "home")


def test_a_cache_that_lost_its_command_is_installed_again(tmp_path: Path) -> None:
    _installed_cache(tmp_path, with_command=False)
    with (
        patch.object(framework, "get_sdk_nrf_tools_path", return_value=tmp_path),
        patch.object(framework, "_get_toolchain_platform_info", return_value=LINUX),
        patch.object(framework, "download_with_resume", side_effect=_fake_download),
        patch.object(framework, "run_command_ok", side_effect=_fake_install) as install,
    ):
        ensure_nordic_nrfutil({})
    install.assert_called_once()


def test_failed_install_leaves_no_ready_marker(tmp_path: Path) -> None:
    with (
        patch.object(framework, "get_sdk_nrf_tools_path", return_value=tmp_path),
        patch.object(framework, "_get_toolchain_platform_info", return_value=LINUX),
        patch.object(framework, "download_with_resume", side_effect=_fake_download),
        patch.object(framework, "run_command_ok", return_value=False),
        pytest.raises(EsphomeError, match="nrf5sdk-tools"),
    ):
        ensure_nordic_nrfutil({})
    assert not list((tmp_path / "nrfutil").glob(".ready-*"))
