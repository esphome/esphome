"""Tests for esphome.arduino8266.framework (downloads and environment)."""

from __future__ import annotations

import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.arduino8266 import framework
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError


@pytest.fixture(autouse=True)
def _build_path(tmp_path: Path) -> None:
    CORE.build_path = tmp_path


def test_framework_package_version() -> None:
    assert framework.framework_package_version(cv.Version(3, 1, 2)) == "3.30102.0"
    assert framework.framework_package_version(cv.Version(3, 2, 0)) == "3.30200.0"
    # A future major bump needs its own encoding, not a doomed registry lookup
    with pytest.raises(EsphomeError, match="not supported yet"):
        framework.framework_package_version(cv.Version(4, 0, 0))
    # Cores before 3.x cannot build ESPHome (C++20) and are rejected
    with pytest.raises(EsphomeError, match="requires core 3"):
        framework.framework_package_version(cv.Version(2, 7, 4))


def test_format_framework_arduino_version_pins_all_series() -> None:
    """The esp8266 component's PIO source formatter across every encoding
    era, including the 4.x rejection it now shares with the installer."""
    from esphome.components.esp8266 import _format_framework_arduino_version as fmt

    assert fmt(cv.Version(3, 1, 2)) == "~3.30102.0"
    # Pre-3 cores are rejected with the version line anchored
    with pytest.raises(cv.Invalid, match="requires core 3"):
        fmt(cv.Version(2, 7, 4))
    # Anchored to the framework version line, not a bare EsphomeError
    with pytest.raises(cv.Invalid, match="not supported yet") as excinfo:
        fmt(cv.Version(4, 0, 0))
    assert excinfo.value.path == ["version"]


def test_tools_path_default_and_prefix(tmp_path: Path) -> None:
    with patch.dict(os.environ, {"ESPHOME_ARDUINO8266_PREFIX": str(tmp_path)}):
        assert framework.get_arduino8266_tools_path() == tmp_path.resolve()
    # A blank prefix must be treated as unset, not as the CWD
    with patch.dict(os.environ, {"ESPHOME_ARDUINO8266_PREFIX": "  "}):
        path = framework.get_arduino8266_tools_path()
    assert path.name == "arduino8266"
    assert path != Path.cwd()


def test_toolchain_builds_are_pinned() -> None:
    """A release must not be pinned without its checksums."""
    for sha256, size in framework.TOOLCHAIN_BUILDS.values():
        assert len(sha256) == 64
        assert size > 0


def test_toolchain_download() -> None:
    sha256, size = framework.TOOLCHAIN_BUILDS["darwin_arm64"]
    with (
        patch.object(framework, "TOOLCHAIN_VERSION", "1.2.3"),
        patch.object(framework, "get_systype", return_value="darwin_arm64"),
    ):
        download = framework.toolchain_download()
    assert download == (
        (
            "https://github.com/esphome-libs/xtensa-lx106-elf-toolchain/releases/"
            "download/1.2.3/toolchain-xtensa-lx106-elf-1.2.3-darwin_arm64.tar.gz"
        ),
        sha256,
        size,
    )


def test_toolchain_download_unsupported_system() -> None:
    with (
        patch.object(framework, "get_systype", return_value="linux_armv7l"),
        pytest.raises(
            EsphomeError, match=r"linux_armv7l.*darwin_arm64.*toolchain: platformio"
        ),
    ):
        framework.toolchain_download()


def test_check_and_install_mirror_skips_pinned_toolchain(tmp_path: Path) -> None:
    """With a mirror override an unsupported host can bring its own toolchain."""
    with (
        patch.dict(os.environ, {"ESPHOME_ARDUINO8266_PREFIX": str(tmp_path)}),
        patch.object(framework, "ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS", ["http://m"]),
        patch.object(framework, "get_systype", return_value="linux_armv7l"),
        patch.object(framework, "install_package") as mock_install,
        patch.object(framework, "prefetch_packages") as mock_prefetch,
        patch.object(framework, "find_ninja", return_value=tmp_path / "ninja"),
    ):
        framework.check_and_install(cv.Version(3, 1, 2))
    assert mock_prefetch.call_args.args[2] == {}
    assert mock_install.call_args_list[1].kwargs["pinned"] is None


def test_check_and_install_returns_paths(tmp_path: Path) -> None:
    toolchain = framework.Download("http://y/toolchain.tar.gz", "def456", 7)
    with (
        patch.dict(os.environ, {"ESPHOME_ARDUINO8266_PREFIX": str(tmp_path)}),
        patch.object(framework, "toolchain_download", return_value=toolchain),
        patch.object(framework, "install_package") as mock_install,
        patch.object(framework, "prefetch_packages") as mock_prefetch,
        patch.object(framework, "find_ninja", return_value=tmp_path / "ninja"),
    ):
        paths = framework.check_and_install(cv.Version(3, 1, 2))
    assert paths.framework == tmp_path / "frameworks" / "3.30102.0"
    assert paths.toolchain == tmp_path / "toolchains" / framework.TOOLCHAIN_VERSION
    assert paths.ninja == tmp_path / "ninja"
    assert mock_install.call_count == 2
    # Full argument pinning: a copy-paste swap between the two near-identical
    # calls (mirrors, destination) must not stay green
    fw_call, tc_call = mock_install.call_args_list
    assert fw_call.args == (
        framework.FRAMEWORK_PACKAGE,
        "3.30102.0",
        tmp_path / "frameworks" / "3.30102.0",
        framework.ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS,
        tmp_path / "downloads",
    )
    assert fw_call.kwargs == {
        "expect": ("cores/esp8266", "tools/sdk", "libraries"),
        "pinned": None,
    }
    assert tc_call.args == (
        framework.TOOLCHAIN_PACKAGE,
        framework.TOOLCHAIN_VERSION,
        tmp_path / "toolchains" / framework.TOOLCHAIN_VERSION,
        framework.ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS,
        tmp_path / "downloads",
    )
    assert tc_call.kwargs == {
        "expect": ("bin", "xtensa-lx106-elf"),
        "pinned": toolchain,
    }
    # The prefetch sees the same package specs as the installs
    assert mock_prefetch.call_args.args == (
        [
            (
                framework.FRAMEWORK_PACKAGE,
                "3.30102.0",
                tmp_path / "frameworks" / "3.30102.0",
                framework.ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS,
            ),
            (
                framework.TOOLCHAIN_PACKAGE,
                framework.TOOLCHAIN_VERSION,
                tmp_path / "toolchains" / framework.TOOLCHAIN_VERSION,
                framework.ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS,
            ),
        ],
        tmp_path / "downloads",
        {framework.TOOLCHAIN_PACKAGE: toolchain},
    )


def test_get_build_env_prepends_toolchain_bin(tmp_path: Path) -> None:
    with patch.object(framework, "ccache_env", return_value={"CCACHE_DIR": "x"}):
        env = framework.get_build_env(tmp_path, None)
    assert env["PATH"].startswith(str(tmp_path / "bin") + os.pathsep)
    assert env["CCACHE_DIR"] == "x"


def test_check_and_install_rejects_old_core(tmp_path: Path) -> None:
    """Calling the installer below the floor fails before any download."""
    with pytest.raises(EsphomeError, match=">= 3.1.1"):
        framework.check_and_install(cv.Version(3, 0, 2))


def test_get_build_env_without_path_has_no_empty_entry(tmp_path: Path) -> None:
    """An absent PATH must not leave a trailing separator (an empty entry
    means the current directory to the shell)."""
    with (
        patch.dict(os.environ, {}, clear=True),
        patch.object(framework, "ccache_env", return_value={}),
    ):
        env = framework.get_build_env(tmp_path, None)
    assert env["PATH"] == str(tmp_path / "bin")
    with (
        patch.dict(
            os.environ, {"PATH": f"/usr/bin{os.pathsep}{os.pathsep}/bin"}, clear=True
        ),
        patch.object(framework, "ccache_env", return_value={}),
    ):
        env = framework.get_build_env(tmp_path, None)
    assert env["PATH"].split(os.pathsep) == [str(tmp_path / "bin"), "/usr/bin", "/bin"]


def test_toolchain_tool_layout(tmp_path: Path) -> None:
    """One owner for the bin/xtensa-lx106-elf-<name> layout."""
    tool = framework.toolchain_tool(tmp_path, "addr2line")
    assert tool.parent == tmp_path / "bin"
    assert tool.name.startswith("xtensa-lx106-elf-addr2line")
    assert (tool.suffix == ".exe") is (os.name == "nt")


def test_get_build_env_uses_the_arduino8266_ccache_dir(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ESPHOME_ARDUINO8266_PREFIX", str(tmp_path / "cache"))
    monkeypatch.delenv("CCACHE_DIR", raising=False)
    env = framework.get_build_env(tmp_path / "toolchain", "/usr/bin/ccache")
    assert env["CCACHE_DIR"] == str((tmp_path / "cache").resolve() / "ccache")
    # None means resolved and disabled
    assert "CCACHE_DIR" not in framework.get_build_env(tmp_path / "toolchain", None)
