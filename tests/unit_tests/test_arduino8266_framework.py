"""Tests for esphome.arduino8266.framework (downloads and environment)."""

from __future__ import annotations

from contextlib import contextmanager
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import MagicMock, patch

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


@pytest.mark.parametrize(
    ("system", "machine", "expected"),
    [
        ("Darwin", "arm64", "darwin_arm64"),
        ("Darwin", "x86_64", "darwin_x86_64"),
        ("Windows", "AMD64", "windows_amd64"),
        ("Windows", "ARM64", "windows_amd64"),
        ("Windows", "x86", "windows_x86"),
        ("Linux", "x86_64", "linux_x86_64"),
        ("Linux", "aarch64", "linux_aarch64"),
        ("Linux", "i686", "linux_i686"),
        ("Linux", "armv7l", "linux_armv7l"),
    ],
)
def test_pio_system(system: str, machine: str, expected: str) -> None:
    with (
        patch("platform.system", return_value=system),
        patch("platform.machine", return_value=machine),
    ):
        assert framework._pio_system() == expected


@pytest.mark.parametrize(
    ("system", "machine"),
    [
        ("FreeBSD", "amd64"),
        ("Linux", "ppc64le"),
        ("Darwin", "ppc"),
        ("Darwin", ""),
        ("Windows", "ia64"),
    ],
)
def test_pio_system_unsupported_host_raises(system: str, machine: str) -> None:
    # Fails at resolution rather than installing a toolchain that can't run
    with (
        patch("platform.system", return_value=system),
        patch("platform.machine", return_value=machine),
        pytest.raises(EsphomeError, match="use 'toolchain: platformio'"),
    ):
        framework._pio_system()


def _registry_response(files: list[dict]) -> MagicMock:
    resp = MagicMock()
    resp.json.return_value = {"versions": [{"name": "1.0.0", "files": files}]}
    return resp


def test_registry_download_network_error_is_clean_and_retried() -> None:
    """Registry failures raise EsphomeError after retries, not a traceback."""
    import requests

    with (
        patch("requests.get", side_effect=requests.ConnectionError("boom")) as mock_get,
        patch.object(framework.time, "sleep") as mock_sleep,
        pytest.raises(EsphomeError, match="Could not query the package registry"),
    ):
        framework._registry_download("pkg", "1.0.0")
    assert mock_get.call_count == 3
    # Backed-off retries, not one burst
    assert mock_sleep.call_count == 3


def test_registry_download_retries_transient_error() -> None:
    import requests

    resp = _registry_response(
        [
            {
                "system": ["linux_x86_64"],
                "download_url": "http://x/linux",
                "checksum": {"sha256": "abc123"},
                "size": 42,
            }
        ]
    )
    with (
        patch("requests.get", side_effect=[requests.ConnectionError("boom"), resp]),
        patch.object(framework.time, "sleep"),
        patch.object(framework, "_pio_system", return_value="linux_x86_64"),
    ):
        assert framework._registry_download("pkg", "1.0.0") == (
            "http://x/linux",
            "abc123",
            42,
        )


def test_registry_download_matches_system() -> None:
    resp = _registry_response(
        [
            {"system": ["windows_amd64"], "download_url": "http://x/win"},
            {
                "system": ["linux_x86_64"],
                "download_url": "http://x/linux",
                "checksum": {"sha256": "abc123"},
                "size": 42,
            },
        ]
    )
    with (
        patch("requests.get", return_value=resp),
        patch.object(framework, "_pio_system", return_value="linux_x86_64"),
    ):
        assert framework._registry_download("pkg", "1.0.0") == (
            "http://x/linux",
            "abc123",
            42,
        )


def test_registry_download_bare_string_system() -> None:
    """A bare-string system tag is an exact match, not a substring test."""
    resp = _registry_response(
        [
            {"system": "linux_x86", "download_url": "http://x/x86"},
            {
                "system": "linux_x86_64",
                "download_url": "http://x/x86_64",
                "checksum": {"sha256": "abc"},
            },
        ]
    )
    with (
        patch("requests.get", return_value=resp),
        patch.object(framework, "_pio_system", return_value="linux_x86_64"),
    ):
        assert framework._registry_download("pkg", "1.0.0")[0] == "http://x/x86_64"


def test_registry_download_wildcard_system() -> None:
    resp = _registry_response(
        [
            {
                "system": "*",
                "download_url": "http://x/any",
                "checksum": {"sha256": "abc"},
                "size": 7,
            }
        ]
    )
    with patch("requests.get", return_value=resp):
        assert framework._registry_download("pkg", "1.0.0") == (
            "http://x/any",
            "abc",
            7,
        )


def test_registry_download_missing_checksum_raises() -> None:
    """An unverifiable archive is refused, never silently extracted."""
    resp = _registry_response([{"system": "*", "download_url": "http://x/any"}])
    with (
        patch("requests.get", return_value=resp),
        pytest.raises(EsphomeError, match="no sha256"),
    ):
        framework._registry_download("pkg", "1.0.0")


def test_registry_download_no_system_match() -> None:
    resp = _registry_response(
        [{"system": ["windows_amd64"], "download_url": "http://x/win"}]
    )
    with (
        patch("requests.get", return_value=resp),
        patch.object(framework, "_pio_system", return_value="linux_x86_64"),
        pytest.raises(EsphomeError, match="No pkg 1.0.0 build"),
    ):
        framework._registry_download("pkg", "1.0.0")


def test_registry_download_version_not_found() -> None:
    resp = MagicMock()
    resp.json.return_value = {"versions": [{"name": "2.0.0", "files": []}]}
    with (
        patch("requests.get", return_value=resp),
        pytest.raises(EsphomeError, match="not found"),
    ):
        framework._registry_download("pkg", "1.0.0")


def test_install_package_skips_when_marker_exists(tmp_path: Path) -> None:
    dest = tmp_path / "pkg"
    dest.mkdir()
    (dest / ".esphome_extracted").touch()
    with patch.object(framework, "download_from_mirrors") as mock_download:
        framework._install_package("pkg", "1.0.0", dest, [])
    mock_download.assert_not_called()


def test_install_package_downloads_via_mirrors(tmp_path: Path) -> None:
    dest = tmp_path / "pkg"
    mirrors = ["http://mirror/{VERSION}/{SYSTEM}.tar.gz"]
    with (
        patch.object(framework, "download_from_mirrors") as mock_download,
        patch.object(framework, "archive_extract_all") as mock_extract,
        patch.object(framework, "_pio_system", return_value="linux_x86_64"),
    ):
        # Extraction is expected to create the directory
        mock_extract.side_effect = lambda *_a, **_kw: dest.mkdir()
        framework._install_package("pkg", "1.0.0", dest, mirrors)
    assert mock_download.call_args[0][0] is mirrors
    assert mock_download.call_args[0][1] == {
        "VERSION": "1.0.0",
        "SYSTEM": "linux_x86_64",
    }
    assert (dest / ".esphome_extracted").is_file()


def test_install_package_downloads_via_registry(tmp_path: Path) -> None:
    """The registry path downloads with the registry's sha256 and size."""
    dest = tmp_path / "pkg"
    with (
        patch.object(framework, "download_with_resume") as mock_download,
        patch.object(framework, "archive_extract_all") as mock_extract,
        patch.object(
            framework,
            "_registry_download",
            return_value=("http://x/pkg.tar.gz", "abc123", 42),
        ),
    ):
        mock_extract.side_effect = lambda *_a, **_kw: dest.mkdir()
        framework._install_package("pkg", "1.0.0", dest, [])
    assert mock_download.call_args[0][0] == "http://x/pkg.tar.gz"
    assert mock_download.call_args[1] == {"sha256": "abc123", "size": 42}


def test_find_ninja_prefers_path(tmp_path: Path) -> None:
    with patch("shutil.which", return_value=str(tmp_path / "ninja")):
        assert framework._find_ninja() == tmp_path / "ninja"


def test_find_ninja_falls_back_to_wheel(tmp_path: Path) -> None:
    """Without a PATH entry, the ninja PyPI wheel's binary is used."""
    binary_name = "ninja.exe" if os.name == "nt" else "ninja"
    (tmp_path / binary_name).touch()
    wheel = MagicMock(BIN_DIR=str(tmp_path))
    with (
        patch("shutil.which", return_value=None),
        patch.dict(sys.modules, {"ninja": wheel}),
    ):
        assert framework._find_ninja() == tmp_path / binary_name


def test_find_ninja_package_not_installed() -> None:
    """A missing ninja package raises the actionable message, not ImportError."""
    with (
        patch("shutil.which", return_value=None),
        patch.dict(sys.modules, {"ninja": None}),
        pytest.raises(EsphomeError, match="ninja not found"),
    ):
        framework._find_ninja()


def test_find_ninja_missing_everywhere(tmp_path: Path) -> None:
    wheel = MagicMock(BIN_DIR=str(tmp_path))
    with (
        patch("shutil.which", return_value=None),
        patch.dict(sys.modules, {"ninja": wheel}),
        pytest.raises(EsphomeError, match="ninja not found"),
    ):
        framework._find_ninja()


def test_check_and_install_returns_paths(tmp_path: Path) -> None:
    with (
        patch.dict(os.environ, {"ESPHOME_ARDUINO8266_PREFIX": str(tmp_path)}),
        patch.object(framework, "_install_package") as mock_install,
        patch.object(framework, "_find_ninja", return_value=tmp_path / "ninja"),
    ):
        paths = framework.check_and_install(cv.Version(3, 1, 2))
    assert paths["framework_path"] == tmp_path / "frameworks" / "3.30102.0"
    assert (
        paths["toolchain_path"] == tmp_path / "toolchains" / framework.TOOLCHAIN_VERSION
    )
    assert paths["ninja_path"] == tmp_path / "ninja"
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
        patch("esphome.framework_helpers._ccache_runs", side_effect=AssertionError),
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


def test_install_package_validates_expected_layout(tmp_path: Path) -> None:
    """The success marker is only written when the extracted tree is usable."""
    dest = tmp_path / "pkg"
    with (
        patch.object(framework, "download_from_mirrors"),
        patch.object(framework, "archive_extract_all") as mock_extract,
        patch.object(framework, "_pio_system", return_value="linux_x86_64"),
    ):
        mock_extract.side_effect = lambda *_a, **_kw: (dest / "bin").mkdir(parents=True)
        framework._install_package("pkg", "1.0.0", dest, ["http://m"], expect=("bin",))
    assert (dest / ".esphome_extracted").is_file()


def test_install_package_unexpected_layout_raises(tmp_path: Path) -> None:
    dest = tmp_path / "pkg"
    with (
        patch.object(framework, "download_from_mirrors"),
        patch.object(framework, "archive_extract_all") as mock_extract,
        patch.object(framework, "_pio_system", return_value="linux_x86_64"),
        pytest.raises(EsphomeError, match="without the expected bin"),
    ):
        mock_extract.side_effect = lambda *_a, **_kw: dest.mkdir()
        framework._install_package("pkg", "1.0.0", dest, ["http://m"], expect=("bin",))
    assert not (dest / ".esphome_extracted").exists()


def test_install_package_marker_rechecked_under_lock(tmp_path: Path) -> None:
    """A concurrent install finishing while we wait for the lock is detected."""
    dest = tmp_path / "pkg"
    marker = dest / ".esphome_extracted"

    @contextmanager
    def _fake_lock(*_a, **_kw):
        dest.mkdir(parents=True, exist_ok=True)
        marker.touch()
        yield

    with (
        patch("filelock.FileLock", _fake_lock),
        patch.object(framework, "download_from_mirrors") as mock_download,
        patch.object(framework, "rmdir") as mock_rmdir,
    ):
        framework._install_package("pkg", "1.0.0", dest, ["http://m"])
    mock_download.assert_not_called()
    mock_rmdir.assert_not_called()


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


def test_install_package_uses_hard_lock(tmp_path: Path) -> None:
    """The install lock must never degrade to a soft (existence) lock."""
    dest = tmp_path / "pkg"
    with (
        patch("filelock.FileLock") as mock_lock,
        patch.object(framework, "download_from_mirrors"),
        patch.object(framework, "archive_extract_all") as mock_extract,
        patch.object(framework, "_pio_system", return_value="linux_x86_64"),
    ):
        mock_extract.side_effect = lambda *_a, **_kw: dest.mkdir(exist_ok=True)
        framework._install_package("pkg", "1.0.0", dest, ["http://m"])
    assert mock_lock.call_args.kwargs["fallback_to_soft"] is False
