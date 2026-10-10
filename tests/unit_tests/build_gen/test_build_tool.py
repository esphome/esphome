"""Tests for the ninja build-tool helper script."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys
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
    # The rspfile is expanded by the shim (GNU ar would escape backslashes)
    assert mock_run.call_args[0][0] == ["ar-bin", "rcs", str(archive), "a.o"]


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

    src = tmp_path / "a.bin"
    src.write_text("x")
    dst = tmp_path / "b.bin"
    result = subprocess.run(
        [sys.executable, build_tool.__file__, "copy", str(src), str(dst)],
        check=False,
    )
    assert result.returncode == 0
    assert dst.read_text() == "x"


def test_ar_expands_rspfile_without_escaping(tmp_path) -> None:
    """Backslash paths survive: the shim expands the rspfile itself instead
    of letting GNU ar treat backslashes as escapes."""
    rsp = tmp_path / "objs.rsp"
    rsp.write_text("obj/a.o\nsub\\b.o\n")
    with (
        patch.object(
            build_tool.sys,
            "argv",
            ["build_tool", "ar", "ar-bin", str(tmp_path / "lib.a"), str(rsp)],
        ),
        patch.object(
            build_tool.subprocess, "run", return_value=MagicMock(returncode=0)
        ) as mock_run,
    ):
        assert build_tool.main() == 0
    assert mock_run.call_args[0][0] == [
        "ar-bin",
        "rcs",
        str(tmp_path / "lib.a"),
        "obj/a.o",
        "sub\\b.o",
    ]


def test_ar_unquotes_ninja_escaped_paths(tmp_path: Path) -> None:
    """The shim strips a simple surrounding quote, since ninja shell-
    quotes special rsp paths, so ar sees the real filename."""
    rsp = tmp_path / "t.rsp"
    rsp.write_text("'obj/a b.o'\nobj/c.o\n")
    with (
        patch.object(
            build_tool.sys, "argv", ["bt", "ar", "/usr/bin/ar", "lib.a", str(rsp)]
        ),
        patch.object(build_tool.subprocess, "run") as mock_run,
    ):
        mock_run.return_value.returncode = 0
        rc = build_tool.main()
    assert rc == 0
    assert mock_run.call_args.args[0] == [
        "/usr/bin/ar",
        "rcs",
        "lib.a",
        "obj/a b.o",
        "obj/c.o",
    ]


def test_ar_empty_object_list_fails(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A lost object list is an error here, not undefined symbols at link."""
    rsp = tmp_path / "t.rsp"
    rsp.write_text("\n\n")
    with patch.object(
        build_tool.sys, "argv", ["bt", "ar", "/usr/bin/ar", "lib.a", str(rsp)]
    ):
        rc = build_tool.main()
    assert rc == 1
    assert "no objects listed" in capsys.readouterr().err


def test_ar_batches_long_object_lists(tmp_path: Path) -> None:
    """The expanded argv must stay under the Windows 32767-char limit: a
    long object list creates with rcs, then appends with qs."""
    archive = tmp_path / "lib.a"
    rsp = tmp_path / "lib.a.rsp"
    objects = [f"dir/{'x' * 120}_{i}.o" for i in range(400)]
    rsp.write_text("\n".join(objects) + "\n")
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
    calls = [c[0][0] for c in mock_run.call_args_list]
    assert len(calls) > 1
    assert calls[0][1] == "rcs"
    assert all(c[1] == "qs" for c in calls[1:])
    assert [o for c in calls for o in c[3:]] == objects
    assert all(sum(len(a) + 1 for a in c) < 32000 for c in calls)


def test_ar_batch_failure_stops(tmp_path: Path) -> None:
    """A failing batch propagates its exit code without running the rest."""
    archive = tmp_path / "lib.a"
    rsp = tmp_path / "lib.a.rsp"
    rsp.write_text("\n".join(f"{'y' * 200}_{i}.o" for i in range(300)) + "\n")
    with (
        patch.object(
            build_tool.sys,
            "argv",
            ["build_tool", "ar", "ar-bin", str(archive), str(rsp)],
        ),
        patch.object(
            build_tool.subprocess,
            "run",
            side_effect=lambda cmd, **kw: (
                archive.write_text("partial"),
                MagicMock(returncode=3),
            )[1],
        ) as mock_run,
    ):
        assert build_tool.main() == 3
    assert mock_run.call_count == 1
    # The failed batch must not leave a truncated archive behind
    assert not archive.exists()


def test_ar_exception_leaves_no_partial_archive(tmp_path: Path) -> None:
    """A missing ar binary mid-loop must not leave a truncated archive from
    earlier successful batches."""
    archive = tmp_path / "lib.a"
    rsp = tmp_path / "lib.a.rsp"
    rsp.write_text("a.o\n")
    with (
        patch.object(
            build_tool.sys,
            "argv",
            ["build_tool", "ar", "ar-bin", str(archive), str(rsp)],
        ),
        patch.object(
            build_tool.subprocess,
            "run",
            side_effect=lambda cmd, **kw: (
                archive.write_text("partial"),
                (_ for _ in ()).throw(FileNotFoundError("no ar")),
            ),
        ),
        pytest.raises(FileNotFoundError),
    ):
        build_tool.main()
    assert not archive.exists()


def test_surplus_arguments_error(capsys: pytest.CaptureFixture[str]) -> None:
    """A mis-specified ninja rule passing extra operands errors instead of
    silently dropping them."""
    with patch.object(
        build_tool.sys, "argv", ["build_tool", "copy", "a", "b", "extra"]
    ):
        assert build_tool.main() == 1
    assert "expected 2 arguments, got 3" in capsys.readouterr().err


def test_copy_same_file_keeps_the_input(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A same-file copy (dst IS src) must not unlink the input, and fails
    with a message and exit code like the other shim paths."""
    src = tmp_path / "firmware.bin"
    src.write_bytes(b"image")
    with patch.object(
        build_tool.sys, "argv", ["build_tool", "copy", str(src), str(src)]
    ):
        assert build_tool.main() == 1
    assert src.read_bytes() == b"image"
    assert "failed" in capsys.readouterr().err


def test_copy_failure_leaves_no_partial_output(tmp_path: Path) -> None:
    """A failed copy unlinks the destination; a partial firmware image must
    never be left on disk."""
    dst = tmp_path / "firmware.factory.bin"
    dst.write_text("stale")
    with (
        patch.object(build_tool.shutil, "copyfile", side_effect=OSError("disk full")),
        patch.object(
            build_tool.sys,
            "argv",
            ["build_tool", "copy", str(tmp_path / "src.bin"), str(dst)],
        ),
    ):
        assert build_tool.main() == 1
    assert not dst.exists()
