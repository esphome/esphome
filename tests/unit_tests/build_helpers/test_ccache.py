"""Tests for the shared ccache policy in esphome.build_helpers.ccache."""

from __future__ import annotations

import os
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from esphome.build_helpers import ccache
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


@pytest.mark.parametrize(
    ("banner", "expected"),
    [
        ("ccache version 4.5.1\nFeatures: file-storage\n", (4, 5, 1)),
        ("ccache version 4.7\n", (4, 7)),
        ("ccache version 4.13.6\n", (4, 13, 6)),
        ("something else\n", None),
        (None, None),
    ],
)
def test_ccache_version(banner: str | None, expected: tuple[int, ...] | None) -> None:
    with patch.object(ccache, "tool_version_output", return_value=banner):
        assert ccache.ccache_version("/usr/bin/ccache") == expected


def test_version_probe_runs_once() -> None:
    """The runnability probe and the version check share one spawn."""
    with (
        patch.dict(os.environ, {}, clear=True),
        patch("shutil.which", return_value="/usr/bin/ccache"),
        patch.object(
            ccache, "tool_version_output", return_value="ccache version 4.6.3\n"
        ) as mock_probe,
    ):
        assert ccache.resolve_ccache_path() == "/usr/bin/ccache"
        assert ccache.ccache_breaks_pch()
        assert ccache.ccache_breaks_pch()
    mock_probe.assert_called_once()


@pytest.mark.parametrize(
    ("banner", "expected"),
    [
        ("ccache version 4.2\n", True),
        ("ccache version 4.6.3\n", True),
        ("ccache version 4.7\n", False),
        ("ccache version 4.9.1\n", False),
        ("garbage\n", False),
    ],
)
def test_ccache_breaks_pch(banner: str, expected: bool) -> None:
    with (
        patch.dict(os.environ, {}, clear=True),
        patch("shutil.which", return_value="/usr/bin/ccache"),
        patch.object(ccache, "tool_version_output", return_value=banner),
    ):
        assert ccache.ccache_breaks_pch() is expected


def test_ccache_breaks_pch_logs_once(caplog: pytest.LogCaptureFixture) -> None:
    with (
        patch.dict(os.environ, {}, clear=True),
        patch("shutil.which", return_value="/usr/bin/ccache"),
        patch.object(
            ccache, "tool_version_output", return_value="ccache version 4.5.1\n"
        ),
        caplog.at_level("INFO"),
    ):
        assert ccache.ccache_breaks_pch()
        assert ccache.ccache_breaks_pch()
    assert caplog.text.count("ccache 4.5.1 at /usr/bin/ccache") == 1
    assert "upgrade ccache to 4.7 or newer" in caplog.text


def test_ccache_breaks_pch_without_ccache() -> None:
    with (
        patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": "0"}, clear=True),
        patch.object(ccache, "tool_version_output", side_effect=AssertionError),
    ):
        assert not ccache.ccache_breaks_pch()
    with (
        patch.dict(os.environ, {}, clear=True),
        patch("shutil.which", return_value=None),
    ):
        assert not ccache.ccache_breaks_pch()


def test_ccache_breaks_pch_explicit_enable_reads_version() -> None:
    """ESPHOME_CCACHE_ENABLE=1 skips the runnability probe, not the version."""
    with (
        patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": "1"}, clear=True),
        patch("shutil.which", return_value="/usr/bin/ccache"),
        patch.object(
            ccache, "tool_version_output", return_value="ccache version 4.6.3\n"
        ),
    ):
        assert ccache.ccache_breaks_pch()
