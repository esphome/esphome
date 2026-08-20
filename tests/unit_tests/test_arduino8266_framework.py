"""Tests for esphome.arduino8266.framework (downloads and environment)."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
from unittest.mock import patch

import pytest

from esphome.arduino8266 import framework
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError


@pytest.fixture(autouse=True)
def _clear_caches(tmp_path: Path) -> None:
    framework.ccache_path.cache_clear()
    CORE.build_path = tmp_path


def test_framework_package_version() -> None:
    assert framework.framework_package_version(cv.Version(3, 1, 2)) == "3.30102.0"
    assert framework.framework_package_version(cv.Version(3, 2, 0)) == "3.30200.0"


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
        patch.object(framework, "find_ninja", return_value=tmp_path / "ninja"),
    ):
        paths = framework.check_and_install(cv.Version(3, 1, 2))
    assert paths.framework == tmp_path / "frameworks" / "3.30102.0"
    assert paths.toolchain == tmp_path / "toolchains" / framework.TOOLCHAIN_VERSION
    assert paths.ninja == tmp_path / "ninja"
    assert mock_install.call_count == 2
    # The layout checks cover the directories write_project needs, including
    # the bundled libraries/ tree
    fw_expect = mock_install.call_args_list[0].kwargs["expect"]
    assert fw_expect == ("cores/esp8266", "tools/sdk", "libraries")
    assert mock_install.call_args_list[1].kwargs["expect"] == ("bin",)


def test_get_build_env_prepends_toolchain_bin(tmp_path: Path) -> None:
    with patch.object(framework, "ccache_env", return_value={"CCACHE_DIR": "x"}):
        env = framework.get_build_env(tmp_path)
    assert env["PATH"].startswith(str(tmp_path / "bin") + os.pathsep)
    assert env["CCACHE_DIR"] == "x"


def test_ccache_path_disabled_by_env() -> None:
    with patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": "0"}):
        assert framework.ccache_path() is None


def test_ccache_path_no_binary(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("ESPHOME_CCACHE_ENABLE", raising=False)
    with patch("shutil.which", return_value=None):
        assert framework.ccache_path() is None


def test_ccache_path_probe_failure(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("ESPHOME_CCACHE_ENABLE", raising=False)
    with (
        patch("shutil.which", return_value="/usr/bin/ccache"),
        patch("subprocess.run", side_effect=subprocess.SubprocessError),
    ):
        assert framework.ccache_path() is None


def test_ccache_path_ok(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("ESPHOME_CCACHE_ENABLE", raising=False)
    with (
        patch("shutil.which", return_value="/usr/bin/ccache"),
        patch("subprocess.run"),
    ):
        assert framework.ccache_path() == "/usr/bin/ccache"


def test_ccache_path_explicit_missing_binary_warns(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    monkeypatch.setenv("ESPHOME_CCACHE_ENABLE", "1")
    with patch("shutil.which", return_value=None):
        assert framework.ccache_path() is None
    assert "no ccache binary is on PATH" in caplog.text


def test_ccache_path_explicit_skips_probe(monkeypatch: pytest.MonkeyPatch) -> None:
    """An explicit opt-in trusts the binary without the runnability probe."""
    monkeypatch.setenv("ESPHOME_CCACHE_ENABLE", "1")
    with (
        patch("shutil.which", return_value="/usr/bin/ccache"),
        patch("esphome.build_helpers.ccache._ccache_runs", side_effect=AssertionError),
    ):
        assert framework.ccache_path() == "/usr/bin/ccache"


def test_ccache_env(tmp_path: Path) -> None:
    with patch.object(framework, "ccache_path", return_value=None):
        assert framework.ccache_env() == {}
    with (
        patch.object(framework, "ccache_path", return_value="/usr/bin/ccache"),
        patch.dict(os.environ, {"CCACHE_NOHASHDIR": "false"}),
    ):
        env = framework.ccache_env()
    # User-set values are respected; the rest get defaults
    assert "CCACHE_NOHASHDIR" not in env
    assert env["CCACHE_DEPEND"] == "1"
    assert env["CCACHE_BASEDIR"] == str(Path(CORE.build_path).resolve())
    assert env["CCACHE_DIR"].endswith("ccache")


def test_ccache_env_requires_build_path() -> None:
    """Building the env before preload set build_path fails loudly."""
    CORE.build_path = None
    with (
        patch.object(framework, "ccache_path", return_value="/cc/ccache"),
        pytest.raises(ValueError, match="build_path"),
    ):
        framework.ccache_env()


def test_check_and_install_rejects_old_core(tmp_path: Path) -> None:
    """Calling the installer below the floor fails before any download."""
    with pytest.raises(EsphomeError, match=">= 3.1.1"):
        framework.check_and_install(cv.Version(3, 0, 2))
