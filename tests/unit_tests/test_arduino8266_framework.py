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
    # 2.6.3+ cores use the same package-major-3 encoding (PlatformIO path)
    assert framework.framework_package_version(cv.Version(2, 7, 4)) == "3.20704.0"
    # A future major bump needs its own encoding, not a doomed registry lookup
    with pytest.raises(EsphomeError, match="not supported yet"):
        framework.framework_package_version(cv.Version(4, 0, 0))
    # The boundary matches the PlatformIO era guard; a 2.6.2 pre-release
    # keeps this encoding
    with pytest.raises(EsphomeError, match="older package encoding"):
        framework.framework_package_version(cv.Version(2, 6, 2))
    assert framework.framework_package_version(cv.Version(2, 6, 2, "b1")) == "3.20602.0"
    assert framework.framework_package_version(cv.Version(2, 6, 3)) == "3.20603.0"


def test_format_framework_arduino_version_pins_all_series() -> None:
    """The esp8266 component's PIO source formatter across every encoding
    era, including the 4.x rejection it now shares with the installer."""
    from esphome.components.esp8266 import _format_framework_arduino_version as fmt

    assert fmt(cv.Version(2, 4, 1)) == "~1.20401.0"
    assert fmt(cv.Version(2, 6, 2)) == "~2.20602.0"
    assert fmt(cv.Version(2, 7, 4)) == "~3.20704.0"
    assert fmt(cv.Version(3, 1, 2)) == "~3.30102.0"
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


def test_check_and_install_returns_paths(tmp_path: Path) -> None:
    with (
        patch.dict(os.environ, {"ESPHOME_ARDUINO8266_PREFIX": str(tmp_path)}),
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
    assert fw_call.kwargs["expect"] == ("cores/esp8266", "tools/sdk", "libraries")
    assert tc_call.args == (
        framework.TOOLCHAIN_PACKAGE,
        framework.TOOLCHAIN_VERSION,
        tmp_path / "toolchains" / framework.TOOLCHAIN_VERSION,
        framework.ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS,
        tmp_path / "downloads",
    )
    assert tc_call.kwargs["expect"] == ("bin", "xtensa-lx106-elf")
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
    )


def test_get_build_env_prepends_toolchain_bin(tmp_path: Path) -> None:
    with patch.object(framework, "ccache_env", return_value={"CCACHE_DIR": "x"}):
        env = framework.get_build_env(tmp_path, None)
    assert env["PATH"].startswith(str(tmp_path / "bin") + os.pathsep)
    assert env["CCACHE_DIR"] == "x"


def test_ccache_env(tmp_path: Path) -> None:
    assert framework.ccache_env(None) == {}
    with patch.dict(os.environ, {"CCACHE_NOHASHDIR": "false"}, clear=True):
        env = framework.ccache_env("/usr/bin/ccache")
    # User-set values are respected; the rest get defaults
    assert "CCACHE_NOHASHDIR" not in env
    assert env["CCACHE_DEPEND"] == "1"
    assert env["CCACHE_BASEDIR"] == str(Path(CORE.build_path).resolve())
    assert env["CCACHE_DIR"].endswith("ccache")


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


def test_ccache_env_accepts_a_preresolved_path() -> None:
    """The caller resolves ccache once and threads it through; None means
    resolved-and-disabled."""
    with patch.dict(os.environ, {}, clear=True):
        assert framework.ccache_env(None) == {}
        env = framework.ccache_env("/usr/bin/ccache")
    assert env["CCACHE_DIR"].endswith("ccache")


def test_toolchain_tool_layout(tmp_path: Path) -> None:
    """One owner for the bin/xtensa-lx106-elf-<name> layout."""
    tool = framework.toolchain_tool(tmp_path, "addr2line")
    assert tool.parent == tmp_path / "bin"
    assert tool.name.startswith("xtensa-lx106-elf-addr2line")
    assert (tool.suffix == ".exe") is (os.name == "nt")
