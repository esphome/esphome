"""Unit tests for esphome.components.zephyr.sdk_install."""

from __future__ import annotations

from collections.abc import Iterator
from pathlib import Path
import re
from typing import Any
from unittest.mock import MagicMock, patch

import pytest

from esphome.components.zephyr import sdk_install
from esphome.components.zephyr.sdk_install import (
    ARM_TOOLCHAIN,
    get_sdk_host_info,
    install_sdk,
    sdk_toolchain_path,
)
from esphome.core import EsphomeError

TOOLCHAIN = "riscv64-zephyr-elf"
ARM_ONLY_MIRROR = (
    "https://mirror.example/v{VERSION}/"
    "toolchain_gnu_{sysname}-{machine}_arm-zephyr-eabi.{extension}"
)


@pytest.mark.parametrize(
    ("system", "machine", "expected"),
    [
        # default — no branch hit
        ("Linux", "x86_64", ("linux", "x86_64", "tar.xz")),
        # arm64 → aarch64 rename
        ("Linux", "arm64", ("linux", "aarch64", "tar.xz")),
        # darwin → macos rename only
        ("Darwin", "x86_64", ("macos", "x86_64", "tar.xz")),
        # both renames apply
        ("Darwin", "arm64", ("macos", "aarch64", "tar.xz")),
        # windows forces x86_64 + 7z; arm64 rename is overwritten
        ("Windows", "arm64", ("windows", "x86_64", "7z")),
    ],
)
def test_get_sdk_host_info(
    system: str, machine: str, expected: tuple[str, str, str]
) -> None:
    with (
        patch("platform.system", return_value=system),
        patch("platform.machine", return_value=machine),
    ):
        assert get_sdk_host_info() == expected


@pytest.mark.parametrize(
    ("version", "expected"),
    [
        ("0.17.4", Path("sdk") / TOOLCHAIN),
        ("1.0.1", Path("sdk") / "gnu" / TOOLCHAIN),
    ],
)
def test_sdk_toolchain_path_follows_the_sdk_layout(
    version: str, expected: Path
) -> None:
    assert sdk_toolchain_path(Path("sdk"), version, TOOLCHAIN) == expected


def test_toolchain_mirror_names_the_toolchain() -> None:
    """The default URL is the sdk-ng asset of the requested toolchain."""
    (template,) = sdk_install.SDK_NG_TOOLCHAIN_GNU_MIRRORS
    assert template.format(
        VERSION="1.0.1",
        sysname="windows",
        machine="x86_64",
        extension="7z",
        toolchain=TOOLCHAIN,
    ) == (
        "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v1.0.1/"
        f"toolchain_gnu_windows-x86_64_{TOOLCHAIN}.7z"
    )


# ---------------------------------------------------------------------------
# install_sdk
# ---------------------------------------------------------------------------


@pytest.fixture
def mock_download() -> Iterator[MagicMock]:
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


def test_install_sdk_downloads_minimal_and_toolchain(
    tmp_path: Path, mock_download: MagicMock
) -> None:
    sdk_path = tmp_path / "toolchains" / "1.0.1"

    install_sdk(sdk_path, "1.0.1", TOOLCHAIN)

    (minimal, toolchain) = mock_download.call_args_list
    assert minimal.args[0] == sdk_install.SDK_NG_MINIMAL_MIRRORS
    assert minimal.args[2] == sdk_path.with_name("1.0.1.minimal.archive")
    assert minimal.args[3] == sdk_path
    assert toolchain.args[0] == sdk_install.SDK_NG_TOOLCHAIN_GNU_MIRRORS
    assert toolchain.args[1] == {
        "VERSION": "1.0.1",
        "sysname": "linux",
        "machine": "x86_64",
        "extension": "tar.xz",
        "toolchain": TOOLCHAIN,
    }
    assert toolchain.args[2] == sdk_path.with_name("1.0.1.toolchain.archive")
    assert toolchain.args[3] == sdk_path / "gnu" / TOOLCHAIN


def test_install_sdk_uses_the_0x_mirrors_before_sdk_1(
    tmp_path: Path, mock_download: MagicMock
) -> None:
    sdk_path = tmp_path / "toolchains" / "0.17.4"

    install_sdk(sdk_path, "0.17.4", TOOLCHAIN)

    toolchain = mock_download.call_args_list[1]
    assert toolchain.args[0] == sdk_install.SDK_NG_TOOLCHAIN_MIRRORS
    assert toolchain.args[3] == sdk_path / TOOLCHAIN


def test_install_sdk_removes_an_unfinished_install(
    tmp_path: Path, mock_download: MagicMock
) -> None:
    """A half-extracted toolchain is removed before installing again."""
    sdk_path = tmp_path / "toolchains" / "1.0.1"
    leftover = sdk_path / "gnu" / TOOLCHAIN / "half-extracted"
    leftover.parent.mkdir(parents=True)
    leftover.touch()

    install_sdk(sdk_path, "1.0.1", TOOLCHAIN)

    assert not leftover.exists()
    assert mock_download.call_count == 2


def test_install_sdk_stops_when_cleanup_fails(
    tmp_path: Path, mock_download: MagicMock
) -> None:
    sdk_path = tmp_path / "toolchains" / "1.0.1"
    sdk_path.mkdir(parents=True)

    with (
        patch("esphome.framework_helpers.rmtree", side_effect=OSError("busy")),
        pytest.raises(RuntimeError, match="Please remove it manually"),
    ):
        install_sdk(sdk_path, "1.0.1", TOOLCHAIN)

    mock_download.assert_not_called()


@pytest.mark.parametrize(
    "template",
    [
        ARM_ONLY_MIRROR,
        # Even its own name hard-coded: only {toolchain} proves the override is current
        (
            "https://mirror.example/v{VERSION}/"
            "toolchain_gnu_{sysname}-{machine}_riscv64-zephyr-elf.{extension}"
        ),
    ],
)
def test_install_sdk_refuses_a_mirror_without_toolchain_for_other_toolchains(
    tmp_path: Path, mock_download: MagicMock, template: str
) -> None:
    """An override written for nrf52's Arm toolchain must not install Arm as RISC-V."""
    sdk_path = tmp_path / "toolchains" / "1.0.1"
    sdk_path.mkdir(parents=True)

    with (
        patch.object(sdk_install, "SDK_NG_TOOLCHAIN_GNU_MIRRORS", [template]),
        pytest.raises(
            EsphomeError,
            match=re.escape(
                "ESPHOME_SDK_NG_TOOLCHAIN_GNU_MIRRORS has no {toolchain}, "
                f"so it can't download {TOOLCHAIN}"
            ),
        ),
    ):
        install_sdk(sdk_path, "1.0.1", TOOLCHAIN)

    mock_download.assert_not_called()
    assert sdk_path.exists()


@pytest.mark.parametrize(
    ("version", "env_name", "default_file"),
    [
        (
            "1.0.1",
            "ESPHOME_SDK_NG_TOOLCHAIN_GNU_MIRRORS",
            "toolchain_gnu_{sysname}-{machine}_{toolchain}.{extension}",
        ),
        (
            "0.17.4",
            "ESPHOME_SDK_NG_TOOLCHAIN_MIRRORS",
            "/toolchain_{sysname}-{machine}_{toolchain}.{extension}",
        ),
    ],
)
def test_install_sdk_refusal_shows_the_matching_default(
    tmp_path: Path,
    mock_download: MagicMock,
    version: str,
    env_name: str,
    default_file: str,
) -> None:
    """The fix hint is the default of the variable at fault, for that SDK layout."""
    attr = env_name.removeprefix("ESPHOME_")
    with (
        patch.object(sdk_install, attr, [ARM_ONLY_MIRROR]),
        pytest.raises(EsphomeError) as err,
    ):
        install_sdk(tmp_path / version, version, TOOLCHAIN)

    assert str(err.value).startswith(f"{env_name} has no {{toolchain}}")
    assert "as in the default https://" in str(err.value)
    assert default_file in str(err.value)


@pytest.mark.parametrize(
    ("version", "attr"),
    [
        ("1.0.1", "SDK_NG_TOOLCHAIN_GNU_MIRRORS"),
        ("0.17.4", "SDK_NG_TOOLCHAIN_MIRRORS"),
    ],
)
@pytest.mark.parametrize(
    "template",
    [ARM_ONLY_MIRROR, "https://intranet.example/arm-toolchain-{VERSION}.tar.xz"],
)
def test_install_sdk_keeps_any_arm_override(
    tmp_path: Path, mock_download: MagicMock, version: str, attr: str, template: str
) -> None:
    """nrf52's existing overrides keep working, whatever their URL looks like."""
    with patch.object(sdk_install, attr, [template]):
        install_sdk(tmp_path / version, version, ARM_TOOLCHAIN)

    assert mock_download.call_args_list[1].args[0] == [template]
