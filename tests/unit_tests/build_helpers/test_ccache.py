"""Tests for the shared ccache policy in esphome.build_helpers.ccache."""

from __future__ import annotations

import os
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from esphome.build_helpers import ccache


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
        patch(
            "esphome.build_helpers.ccache.subprocess.run", side_effect=OSError("boom")
        ),
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
    with patch("esphome.build_helpers.ccache.subprocess.run") as mock_run:
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
