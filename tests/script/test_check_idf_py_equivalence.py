"""Tests for script/check_idf_py_equivalence.py."""

import json
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch

import pytest

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "script"))

import check_idf_py_equivalence as guard  # noqa: E402

from esphome.espidf import toolchain  # noqa: E402


def _make_tree(tmp_path: Path) -> Path:
    tree = tmp_path / "config" / ".esphome" / "build" / "dev"
    build = tree / "build"
    build.mkdir(parents=True)
    (build / "project_description.json").write_text(
        json.dumps(
            {
                "project_name": "dev",
                "idf_path": "/idf/frameworks/5.5.5",
                "target": "esp32",
            }
        )
    )
    (build / "CMakeCache.txt").write_text("CCACHE_ENABLE:UNINITIALIZED=0\n")
    (build / "dev.elf").write_bytes(b"elf")
    (build / ".ninja_log").write_text("# ninja log v7\n1\t2\t0\tesp-idf/a.obj\t0\n")
    (tree / "sdkconfig.dev").write_text("")
    return tree


def _run_check(tree: Path, side_effect) -> tuple[list[str], list[list[str]]]:
    calls: list[list[str]] = []

    def run(cmd: list[str], **kwargs: object) -> subprocess.CompletedProcess:
        calls.append(cmd)
        side_effect(cmd)
        return subprocess.CompletedProcess(cmd, 0, "", "")

    with (
        patch.object(toolchain, "_get_idf_env", return_value={}),
        patch.object(toolchain, "_get_idf_tool", return_value="/py"),
        patch.object(toolchain, "_get_idf_path", return_value=Path("/idf")),
        patch.object(guard.subprocess, "run", side_effect=run),
    ):
        return guard.check(tree), calls


def test_check_passes_when_idf_py_changes_nothing(tmp_path: Path) -> None:
    tree = _make_tree(tmp_path)
    problems, calls = _run_check(tree, lambda cmd: None)
    assert problems == []
    sdkconfig = f"SDKCONFIG={tree / 'sdkconfig.dev'}"
    assert calls == [
        ["/py", str(Path("/idf/tools/idf.py")), "-D", sdkconfig, "reconfigure"],
        ["/py", str(Path("/idf/tools/idf.py")), "-D", sdkconfig, "build"],
    ]


def test_check_reports_changed_files_and_rebuilt_outputs(tmp_path: Path) -> None:
    tree = _make_tree(tmp_path)
    build = tree / "build"

    def drift(cmd: list[str]) -> None:
        if cmd[-1] == "reconfigure":
            (build / "CMakeCache.txt").write_text("CCACHE_ENABLE:UNINITIALIZED=1\n")
        else:
            with (build / ".ninja_log").open("a") as log:
                log.write("3\t4\t0\tesp-idf/b.obj\t0\n5\t6\t0\tbootloader-stamp\t0\n")

    problems, _ = _run_check(tree, drift)
    assert problems == [
        "idf.py changed build/CMakeCache.txt",
        "idf.py rebuilt esp-idf/b.obj",
    ]


def test_check_stops_when_idf_py_fails(tmp_path: Path) -> None:
    tree = _make_tree(tmp_path)
    with (
        patch.object(toolchain, "_get_idf_env", return_value={}),
        patch.object(toolchain, "_get_idf_tool", return_value="/py"),
        patch.object(toolchain, "_get_idf_path", return_value=Path("/idf")),
        patch.object(
            guard.subprocess,
            "run",
            return_value=subprocess.CompletedProcess([], 2, "out\n", "err\n"),
        ),
    ):
        problems = guard.check(tree)
    assert problems == ["idf.py reconfigure failed:\nout\nerr\n"]


@pytest.mark.parametrize(("allow_missing", "rc"), [(True, 0), (False, 1)])
def test_main_without_build_trees(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    allow_missing: bool,
    rc: int,
) -> None:
    argv = ["check", str(tmp_path)] + (["--allow-missing"] if allow_missing else [])
    with patch.object(sys, "argv", argv):
        assert guard.main() == rc
    assert "No native ESP-IDF build tree found" in capsys.readouterr().out


@pytest.mark.parametrize(("problems", "rc"), [([], 0), (["idf.py changed x"], 1)])
def test_main_reports_each_tree(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    problems: list[str],
    rc: int,
) -> None:
    tree = _make_tree(tmp_path)
    with (
        patch.object(sys, "argv", ["check", str(tree)]),
        patch.object(guard, "check", return_value=problems),
    ):
        assert guard.main() == rc
    out = capsys.readouterr().out
    assert f"{tree}: {'DIFFERS' if problems else 'OK'}" in out
    assert ("no longer matches idf.py" in out) is bool(problems)
