"""Tests for esphome.build_helpers.ninja."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import MagicMock, patch

import pytest

from esphome.build_helpers import ninja as ninja_helper
from esphome.core import EsphomeError


def test_find_ninja_prefers_path(tmp_path: Path) -> None:
    with (
        patch("shutil.which", return_value=str(tmp_path / "ninja")),
        patch.object(ninja_helper, "_ninja_runs", return_value=True),
    ):
        assert ninja_helper.find_ninja() == tmp_path / "ninja"


def test_find_ninja_falls_back_to_wheel(tmp_path: Path) -> None:
    """Without a PATH entry, the ninja PyPI wheel's binary is used."""
    binary_name = "ninja.exe" if os.name == "nt" else "ninja"
    (tmp_path / binary_name).touch()
    wheel = MagicMock(BIN_DIR=str(tmp_path))
    with (
        patch("shutil.which", return_value=None),
        patch.dict(sys.modules, {"ninja": wheel}),
    ):
        assert ninja_helper.find_ninja() == tmp_path / binary_name


def test_find_ninja_package_not_installed() -> None:
    """A missing ninja package raises the actionable message, not ImportError."""
    with (
        patch("shutil.which", return_value=None),
        patch.dict(sys.modules, {"ninja": None}),
        pytest.raises(EsphomeError, match="ninja not found"),
    ):
        ninja_helper.find_ninja()


def test_find_ninja_missing_everywhere(tmp_path: Path) -> None:
    wheel = MagicMock(BIN_DIR=str(tmp_path))
    with (
        patch("shutil.which", return_value=None),
        patch.dict(sys.modules, {"ninja": wheel}),
        pytest.raises(EsphomeError, match="ninja not found"),
    ):
        ninja_helper.find_ninja()


def test_escape_ninja_specials() -> None:
    assert ninja_helper.escape("a b:c$d") == "a$ b$:c$$d"


def _q(tok: str) -> str:
    """The platform's shell_token quote wrapper (argv rule on Windows)."""
    return f'"{tok}"' if os.name == "nt" else f"'{tok}'"


def test_quote_arg_windows_argv_rule() -> None:
    # Backslash runs double only before a quote (subprocess.list2cmdline rule)
    assert ninja_helper.quote_arg('-DX=a\\"b c') == '"-DX=a\\\\\\"b c"'
    assert ninja_helper.quote_arg("a b\\") == '"a b\\\\"'


def test_shell_token_quotes_only_when_needed() -> None:
    assert ninja_helper.shell_token("-Os") == "-Os"
    assert ninja_helper.shell_token("-DP=C:\\x y") == _q("-DP=C:\\x y")
    assert ninja_helper.shell_token("plain", force=True) == _q("plain")


def test_shell_token_quotes_shell_metacharacters() -> None:
    """Tokens like -DMASK=(1<<3) must not reach /bin/sh -c bare."""
    assert ninja_helper.shell_token("-DMASK=(1<<3)") == _q("-DMASK=(1<<3)")
    assert ninja_helper.shell_token("-DX=a;b") == _q("-DX=a;b")
    assert ninja_helper.shell_token("-DX=$HOME") == _q("-DX=$$HOME")


def test_shell_token_posix_roundtrips_through_sh() -> None:
    """Backslash runs, $, backticks, and quotes must reach the compiler
    exactly as lexed once ninja un-doubles $$ and /bin/sh strips quotes."""

    if sys.platform == "win32":
        pytest.skip("POSIX sh quoting")
    for tok in ("-DP=a\\\\b", "-DX=$VAR", "-DY=`date`", "-DZ=it's", '-DC="q"'):
        quoted = ninja_helper.shell_token(tok).replace("$$", "$")
        out = subprocess.run(
            ["/bin/sh", "-c", f'printf "%s" {quoted}'],
            capture_output=True,
            text=True,
            check=True,
        )
        assert out.stdout == tok


def test_quote_path_force_quotes() -> None:
    assert ninja_helper.quote_path(Path("a b")) == _q("a b")
    assert ninja_helper.quote_path("simple") == _q("simple")


def test_shell_token_empty_token_is_quoted() -> None:
    """An empty argv element must survive as an explicit pair of quotes."""
    assert ninja_helper.shell_token("") == _q("")


def test_find_ninja_probes_path_hit(tmp_path: Path) -> None:
    """A broken PATH shim falls back to the wheel instead of failing every
    build later."""
    binary_name = "ninja.exe" if os.name == "nt" else "ninja"
    (tmp_path / binary_name).touch()
    wheel = MagicMock(BIN_DIR=str(tmp_path))
    with (
        patch("shutil.which", return_value="/broken/ninja"),
        patch.object(ninja_helper, "_ninja_runs", return_value=False),
        patch.dict(sys.modules, {"ninja": wheel}),
    ):
        assert ninja_helper.find_ninja() == tmp_path / binary_name


def test_ninja_probe_failure_warns(caplog: pytest.LogCaptureFixture) -> None:
    with patch("esphome.framework_helpers.subprocess.run", side_effect=OSError("boom")):
        assert ninja_helper._ninja_runs("/broken/ninja") is False
    assert "failed to run" in caplog.text


def test_ninja_probe_success() -> None:
    with patch("esphome.framework_helpers.subprocess.run") as mock_run:
        assert ninja_helper._ninja_runs("/usr/bin/ninja") is True
    assert mock_run.call_args.kwargs["close_fds"] is False


def test_shell_token_windows_branch_uses_argv_rule() -> None:
    """The nt branch quotes with the CreateProcess argv rule (the ubuntu
    coverage run never takes it naturally)."""
    with patch.object(os, "name", "nt"):
        assert ninja_helper.shell_token("a b") == '"a b"'
        assert ninja_helper.shell_token("", force=True) == '""'
