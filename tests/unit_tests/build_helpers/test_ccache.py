"""Tests for the shared ccache policy in esphome.build_helpers.ccache."""

from __future__ import annotations

from collections.abc import Iterator
from contextlib import contextmanager
import os
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import pytest

from esphome.build_helpers import ccache
from esphome.const import Toolchain
from esphome.core import CORE


def test_resolve_opt_out() -> None:
    with patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": "0"}):
        assert ccache.resolve_ccache_path() is None


def test_resolve_no_binary(caplog: pytest.LogCaptureFixture) -> None:
    with (
        patch.dict(os.environ, {}, clear=True),
        patch("shutil.which", return_value=None),
    ):
        assert ccache.resolve_ccache_path() is None
    assert "no ccache binary" not in caplog.text


def test_resolve_probe_failure() -> None:
    with (
        patch.dict(os.environ, {}, clear=True),
        patch("shutil.which", return_value="/usr/bin/ccache"),
        patch("esphome.framework_helpers.subprocess.run", side_effect=OSError("boom")),
    ):
        assert ccache.resolve_ccache_path() is None


def test_resolve_explicit_skips_probe_and_warns_missing(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with (
        patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": "1"}, clear=True),
        patch("shutil.which", return_value="/usr/bin/ccache"),
        patch.object(ccache, "_ccache_runs", side_effect=AssertionError),
    ):
        assert ccache.resolve_ccache_path() == "/usr/bin/ccache"
    with (
        patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": "1"}, clear=True),
        patch("shutil.which", return_value=None),
    ):
        assert ccache.resolve_ccache_path() is None
    assert "no ccache binary is on PATH" in caplog.text


def test_probe_spawns_with_close_fds_false() -> None:
    with patch("esphome.framework_helpers.subprocess.run") as mock_run:
        assert ccache._ccache_runs("/usr/bin/ccache") is True
    assert mock_run.call_args.kwargs["close_fds"] is False


def test_defaults_env(tmp_path: Path) -> None:
    with (
        patch("esphome.core.CORE", SimpleNamespace(build_path=tmp_path / "b")),
        patch.dict(os.environ, {"CCACHE_NOHASHDIR": "false"}, clear=True),
    ):
        env = ccache.ccache_defaults_env(tmp_path / "cache")
    assert env["CCACHE_DIR"] == str(tmp_path / "cache")
    assert env["CCACHE_DEPEND"] == "1"
    assert "CCACHE_NOHASHDIR" not in env  # user value respected


def test_defaults_env_requires_build_path() -> None:
    with (
        patch("esphome.core.CORE", SimpleNamespace(build_path=None)),
        pytest.raises(ValueError, match="build_path"),
    ):
        ccache.ccache_defaults_env(Path("/x"))


@pytest.mark.parametrize("value", ["no", "off", "false", "0"])
def test_resolve_opt_out_synonyms(value: str) -> None:
    """Every recognized falsy spelling disables ccache."""
    with patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": value}):
        assert ccache.resolve_ccache_path() is None


def test_resolve_unrecognized_value_warns_and_probes(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """An unparsable ESPHOME_CCACHE_ENABLE is treated as unset: it must not
    silently enable ccache or skip the runnability probe."""
    with (
        patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": "enabled"}),
        patch("shutil.which", return_value="/usr/bin/ccache"),
        patch.object(ccache, "_ccache_runs", return_value=False) as mock_probe,
    ):
        assert ccache.resolve_ccache_path() is None
    mock_probe.assert_called_once()
    assert "unrecognized ESPHOME_CCACHE_ENABLE" in caplog.text


@pytest.mark.parametrize(
    ("raw", "expected"),
    [
        ("1", True),
        ("enable", True),
        ("ON", True),
        ("0", False),
        ("disable", False),
        ("Off", False),
        ("maybe", None),
        # ENV KNOB= (Docker/CI) has always read as a disable
        ("", False),
        ("   ", False),
    ],
)
def test_parse_enable_env_spelling_tables(
    monkeypatch: pytest.MonkeyPatch, raw: str, expected: bool | None
) -> None:
    """cv.boolean's spelling tables plus the 1/0 env convention."""
    monkeypatch.setenv("ESPHOME_CCACHE_ENABLE", raw)
    assert ccache.parse_enable_env("ESPHOME_CCACHE_ENABLE") is expected


def test_effective_ccache_basedir_prefers_user_value(tmp_path: Path) -> None:
    CORE.build_path = tmp_path
    # Drive-qualified on Windows: "/custom/base" is not absolute there
    base = "C:\\custom\\base" if os.name == "nt" else "/custom/base"
    with patch.dict(os.environ, {"CCACHE_BASEDIR": base}, clear=True):
        assert ccache.effective_ccache_basedir() == base
    with patch.dict(os.environ, {}, clear=True):
        assert ccache.effective_ccache_basedir() == str(tmp_path.resolve())
    # Degenerate values would strip substrings ccache never rewrites
    for bad in ("", "/", "a/b"):
        with patch.dict(os.environ, {"CCACHE_BASEDIR": bad}, clear=True):
            assert ccache.effective_ccache_basedir() == str(tmp_path.resolve())


def test_resolve_absolute_ccache_path(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Anchor the path: ninja runs from the build directory."""
    monkeypatch.chdir(tmp_path)
    with patch.object(ccache, "resolve_ccache_path", return_value="bin/ccache"):
        resolved = ccache.resolve_absolute_ccache_path()
    assert Path(resolved) == tmp_path / "bin" / "ccache"
    with patch.object(ccache, "resolve_ccache_path", return_value=None):
        assert ccache.resolve_absolute_ccache_path() is None


def test_ccache_env(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    from esphome.core import CORE

    CORE.build_path = tmp_path / "build"
    spec = ("ESPHOME_TEST_PREFIX", "test")
    monkeypatch.setenv("ESPHOME_TEST_PREFIX", str(tmp_path / "cache"))
    monkeypatch.setenv("CCACHE_NOHASHDIR", "false")
    for key in ("CCACHE_DIR", "CCACHE_BASEDIR", "CCACHE_DEPEND"):
        monkeypatch.delenv(key, raising=False)
    # None means resolved and disabled
    assert ccache.ccache_env(None, spec) == {}
    env = ccache.ccache_env("/usr/bin/ccache", spec)
    # User-set values are respected; the rest get defaults
    assert "CCACHE_NOHASHDIR" not in env
    assert env["CCACHE_DEPEND"] == "1"
    assert env["CCACHE_DIR"] == str((tmp_path / "cache").resolve() / "ccache")
    assert env["CCACHE_BASEDIR"] == str((tmp_path / "build").resolve())


def test_ccache_defaults_env_escapes_a_dollar(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A literal $ is doubled, since ccache expands $VAR in its settings."""
    from esphome.core import CORE

    CORE.build_path = tmp_path / "a$b"
    for key in ("CCACHE_DIR", "CCACHE_BASEDIR"):
        monkeypatch.delenv(key, raising=False)
    env = ccache.ccache_defaults_env(tmp_path / "c$d")
    assert env["CCACHE_DIR"].endswith("c$$d")
    assert env["CCACHE_BASEDIR"].endswith("a$$b")


def _pch_ccache_env(tmp_path: Path, environ: dict[str, str]) -> dict[str, str]:
    CORE.build_path = tmp_path / "build"
    spec = ("ESPHOME_TEST_PREFIX", "test")
    environ = {"ESPHOME_TEST_PREFIX": str(tmp_path / "cache"), **environ}
    with patch.dict(os.environ, environ, clear=True):
        return ccache.ccache_env("/usr/bin/ccache", spec)


def test_ccache_env_includes_pch_settings(tmp_path: Path) -> None:
    """A native build exports the ccache settings the pch needs."""
    env = _pch_ccache_env(tmp_path, {})
    assert env["CCACHE_SLOPPINESS"] == "pch_defines,time_macros"
    assert env["CCACHE_PCH_EXTSUM"] == "true"


@contextmanager
def _fake_ccache(
    banner: str | Exception, env: dict[str, str] | None = None
) -> Iterator[MagicMock]:
    """An ESPHome-toolchain build with a ccache on PATH printing ``banner``."""
    with (
        patch.dict(os.environ, env or {}, clear=True),
        patch.object(CORE, "toolchain", None),
        patch("shutil.which", return_value="/usr/bin/ccache"),
        patch.object(ccache, "tool_version_probe", return_value=banner) as probe,
    ):
        yield probe


@pytest.mark.parametrize(
    ("banner", "expected"),
    [
        ("ccache version 4.5.1\nFeatures: file-storage\n", (4, 5, 1)),
        ("ccache version 4.7\n", (4, 7)),
        ("ccache version 4.13.6\n", (4, 13, 6)),
        ("something else\n", None),
        (OSError("boom"), None),
    ],
)
def test_ccache_version(
    banner: str | Exception, expected: tuple[int, ...] | None
) -> None:
    with _fake_ccache(banner):
        assert ccache.ccache_version("/usr/bin/ccache") == expected


@pytest.mark.parametrize(
    ("banner", "env", "expected"),
    [
        ("ccache version 4.2\n", {}, True),
        ("ccache version 4.6.3\n", {}, True),
        # An explicit enable skips the runnability probe, not the version
        ("ccache version 4.6.3\n", {"ESPHOME_CCACHE_ENABLE": "1"}, True),
        ("ccache version 4.6.3\n", {"ESPHOME_CCACHE_ENABLE": "0"}, False),
        ("ccache version 4.7\n", {}, False),
        ("ccache version 4.9.1\n", {}, False),
        ("something else\n", {}, False),
    ],
)
def test_ccache_breaks_pch(banner: str, env: dict[str, str], expected: bool) -> None:
    with _fake_ccache(banner, env):
        assert ccache.ccache_breaks_pch() is expected


def test_ccache_breaks_pch_probes_and_logs_once(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The runnability probe and the version check share one spawn."""
    with _fake_ccache("ccache version 4.5.1\n") as probe, caplog.at_level("INFO"):
        assert ccache.resolve_ccache_path() == "/usr/bin/ccache"
        assert ccache.ccache_breaks_pch()
        assert ccache.ccache_breaks_pch()
    probe.assert_called_once()
    assert caplog.text.count("ccache 4.5.1 at /usr/bin/ccache") == 1
    assert "upgrade ccache to 4.7 or newer" in caplog.text


def test_ccache_breaks_pch_leaves_esp_idf_to_cmake() -> None:
    """On ESP-IDF, CMake checks the ccache IDF finds at configure, so code
    generation runs no ccache and needs no installed framework."""
    with (
        _fake_ccache("ccache version 4.6.3\n") as probe,
        patch.object(CORE, "toolchain", Toolchain.ESP_IDF),
        patch(
            "esphome.espidf.framework.check_esp_idf_install",
            side_effect=AssertionError,
        ),
    ):
        assert not ccache.ccache_breaks_pch()
    probe.assert_not_called()


def test_ccache_breaks_pch_forced_broken_binary_is_quiet(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Forced on, a ccache that fails to run is still used, so the version
    check must not claim the build compiles without it."""
    with _fake_ccache(OSError("boom"), {"ESPHOME_CCACHE_ENABLE": "1"}):
        assert not ccache.ccache_breaks_pch()
    assert "compiling without ccache" not in caplog.text


def test_idf_forced_broken_binary_probes_and_warns_once(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The IDF build env and the pch gate share one spawn and one warning."""
    with (
        _fake_ccache(OSError("boom"), {"IDF_CCACHE_ENABLE": "1"}) as probe,
        patch.object(CORE, "toolchain", Toolchain.ESP_IDF),
    ):
        assert ccache.idf_ccache_path(True) == "/usr/bin/ccache"
        assert not ccache.ccache_breaks_pch()
    probe.assert_called_once()
    assert caplog.text.count("the build will use it anyway") == 1
    assert "compiling without ccache" not in caplog.text
