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
    for watched in (*guard.WATCHED, "build/dev.elf", "build/dev.bin"):
        path = tree / watched
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists():
            path.write_bytes(b"x")
    (build / ".ninja_log").write_text(
        "# ninja log v7\n1\t2\t10\tesp-idf/a.obj\t0\n"
        "1\t2\t10\tbootloader/bootloader.bin\t0\n"
    )
    (tree / "sdkconfig.dev").write_text("")
    return tree


def _run_check(
    tree: Path, side_effect, rc: int = 0
) -> tuple[list[str], list[list[str]]]:
    calls: list[list[str]] = []

    def run(cmd: list[str], **kwargs: object) -> subprocess.CompletedProcess:
        calls.append(cmd)
        side_effect(cmd)
        return subprocess.CompletedProcess(cmd, rc, "out\n", "err\n")

    with (
        patch.object(toolchain, "_get_idf_env", return_value={}),
        patch.object(toolchain, "_get_idf_tool", return_value="/py"),
        patch.object(toolchain, "_get_idf_path", return_value=Path("/idf")),
        patch.object(toolchain, "run_reconfigure", return_value=0),
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
            # Compacted log. The re-logged bootloader byproduct and a stamp
            # are not work; a new object mtime is.
            (build / ".ninja_log").write_text(
                "# ninja log v7\n3\t4\t20\tesp-idf/a.obj\t0\n"
                "5\t6\t30\tbootloader/bootloader.bin\t0\n"
                "5\t6\t30\tbootloader-stamp\t0\n"
            )
            # The bootloader sub-build is judged by its own log.
            (build / "bootloader").mkdir(exist_ok=True)
            (build / "bootloader" / ".ninja_log").write_text(
                "# ninja log v7\n1\t2\t40\tbootloader.elf\t0\n"
            )

    problems, _ = _run_check(tree, drift)
    assert problems == [
        "idf.py changed build/CMakeCache.txt",
        "idf.py rebuilt esp-idf/a.obj",
        "idf.py rebuilt bootloader.elf",
    ]


def test_check_stops_when_idf_py_fails(tmp_path: Path) -> None:
    problems, calls = _run_check(_make_tree(tmp_path), lambda cmd: None, rc=2)
    assert problems == ["idf.py reconfigure failed:\nout\nerr\n"]
    assert len(calls) == 1


@pytest.mark.parametrize(
    ("remove", "problem"),
    [
        ("build/build.ninja", "missing build/build.ninja"),
        ("build/dev.bin", "missing build/dev.bin"),
        ("build/.ninja_log", "missing build/.ninja_log"),
    ],
)
def test_check_fails_when_an_input_is_missing(
    tmp_path: Path, remove: str, problem: str
) -> None:
    """A moved or renamed output must not compare as unchanged."""
    tree = _make_tree(tmp_path)
    (tree / remove).unlink()
    problems, calls = _run_check(tree, lambda cmd: None)
    assert problems == [problem]
    assert calls == []


def test_check_stops_when_esphome_configure_fails(tmp_path: Path) -> None:
    """The baseline is ESPHome's own reconfigure; without it nothing is compared."""
    with (
        patch.object(toolchain, "_get_idf_env", return_value={}),
        patch.object(toolchain, "_get_idf_tool", return_value="/py"),
        patch.object(toolchain, "_get_idf_path", return_value=Path("/idf")),
        patch.object(toolchain, "run_reconfigure", return_value=3),
        patch.object(guard.subprocess, "run") as mock_run,
    ):
        problems = guard.check(_make_tree(tmp_path))
    assert problems == ["ESPHome's CMake configure failed with exit code 3"]
    mock_run.assert_not_called()


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


def test_main_checks_only_the_first_found_tree(tmp_path: Path) -> None:
    """The contract does not depend on the target; one tree per batch is enough."""
    first = _make_tree(tmp_path / "a")
    _make_tree(tmp_path / "b")
    with (
        patch.object(sys, "argv", ["check"]),
        patch.object(guard, "REPO_ROOT", tmp_path),
        patch.object(guard, "DEFAULT_GLOB", "*/config/.esphome/build/*"),
        patch.object(guard, "check", return_value=[]) as mock_check,
    ):
        assert guard.main() == 0
    mock_check.assert_called_once_with(first)
