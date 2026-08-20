"""Tests for the ninja build-tool helper script."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.build_gen import build_tool


def test_ar_removes_stale_archive(tmp_path: Path) -> None:
    archive = tmp_path / "lib.a"
    archive.write_text("stale")
    rsp = tmp_path / "lib.a.rsp"
    rsp.write_text("a.o\n")
    with (
        patch.object(
            build_tool.sys,
            "argv",
            ["build_tool", "ar", "ar-bin", str(archive), str(rsp)],
        ),
        patch.object(
            build_tool.subprocess, "run", return_value=MagicMock(returncode=0)
        ) as mock_run,
    ):
        assert build_tool.main() == 0
    assert not archive.exists()
    assert mock_run.call_args[0][0] == ["ar-bin", "rc", str(archive), f"@{rsp}"]


def test_copy(tmp_path: Path) -> None:
    src = tmp_path / "firmware.bin"
    src.write_text("data")
    dst = tmp_path / "firmware.factory.bin"
    with patch.object(
        build_tool.sys, "argv", ["build_tool", "copy", str(src), str(dst)]
    ):
        assert build_tool.main() == 0
    assert dst.read_text() == "data"


def test_unknown_mode(capsys: pytest.CaptureFixture[str]) -> None:
    with patch.object(build_tool.sys, "argv", ["build_tool", "bogus"]):
        assert build_tool.main() == 1
    assert "unknown build_tool mode" in capsys.readouterr().err


def test_runs_as_script(tmp_path: Path) -> None:
    """The ninja rules invoke the file as a plain script."""
    import subprocess
    import sys

    src = tmp_path / "a.bin"
    src.write_text("x")
    dst = tmp_path / "b.bin"
    result = subprocess.run(
        [sys.executable, build_tool.__file__, "copy", str(src), str(dst)],
        check=False,
    )
    assert result.returncode == 0
    assert dst.read_text() == "x"
