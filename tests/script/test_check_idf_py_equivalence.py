"""Tests for script/check_idf_py_equivalence.py."""

from collections.abc import Callable, Iterator
import json
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch

import pytest

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "script"))

import check_idf_py_equivalence as guard  # noqa: E402

from esphome.build_gen import espidf as build_gen  # noqa: E402
from esphome.core import CORE  # noqa: E402
from esphome.espidf import toolchain  # noqa: E402


@pytest.fixture(autouse=True)
def _reset_core() -> Iterator[None]:
    """check() points the global CORE at the tree it inspects."""
    yield
    CORE.reset()


BOOTLOADER_LOG = "build/bootloader/.ninja_log"
ALL_LOGS = (guard.TOP_NINJA_LOG, BOOTLOADER_LOG)


def _make_tree(tmp_path: Path, skip_bootloader: bool = False) -> Path:
    """A fake build tree; stock shape by default, or the skip shape
    (define set to 1, no bootloader bin, no sub-build)."""
    tree = tmp_path / "config" / ".esphome" / "build" / "dev"
    build = tree / "build"
    files = [
        *guard.watched("dev", skip_bootloader),
        *guard._ninja_logs(skip_bootloader),
    ]
    for name in files:
        (tree / name).parent.mkdir(parents=True, exist_ok=True)
        (tree / name).write_bytes(b"x")
    define = "1" if skip_bootloader else "0"
    (build / "CMakeCache.txt").write_text(
        f"{toolchain.SKIP_BOOTLOADER_DEFINE}:UNINITIALIZED={define}\n"
    )
    (build / "project_description.json").write_text(
        json.dumps(
            {
                "project_name": "dev",
                "idf_path": "/idf/frameworks/5.5.5",
                "target": "esp32",
            }
        )
    )
    (build / ".ninja_log").write_text(
        "# ninja log v7\n1\t2\t10\tesp-idf/a.obj\t0\n"
        "1\t2\t10\tbootloader/bootloader.bin\t0\n"
    )
    if not skip_bootloader:
        (build / "bootloader" / ".ninja_log").write_text(
            "# ninja log v7\n1\t2\t10\tbootloader.elf\t0\n"
        )
    (tree / "sdkconfig.dev").write_text("")
    return tree


def _run_check(
    tree: Path,
    side_effect: Callable[[list[str]], None] = lambda cmd: None,
    rc: int = 0,
    esphome_rcs: tuple[int, int] = (0, 0),
    macro_matches: bool = True,
    envs: list[dict[str, str]] | None = None,
) -> tuple[list[str], list[list[str]]]:
    """Run check() with idf.py replaced by ``side_effect``; return problems, calls.

    ``envs`` collects the env each idf.py call receives.
    """
    calls: list[list[str]] = []

    def run(cmd: list[str], **kwargs: object) -> subprocess.CompletedProcess:
        calls.append(cmd)
        if envs is not None:
            envs.append(kwargs["env"])
        side_effect(cmd)
        return subprocess.CompletedProcess(cmd, rc, "out\n", "err\n")

    with (
        # Snapshot at call time like the real cached env.
        patch.object(
            toolchain, "_get_idf_env", side_effect=lambda *_: dict(os.environ)
        ),
        patch.object(toolchain, "_get_idf_tool", return_value="/py"),
        patch.object(toolchain, "_get_idf_path", return_value=Path("/idf")),
        patch.object(toolchain, "run_reconfigure", return_value=esphome_rcs[0]),
        patch.object(toolchain, "_run_ninja", return_value=esphome_rcs[1]),
        patch.object(build_gen, "idf_macro_matches", return_value=macro_matches),
        patch.object(guard.subprocess, "run", side_effect=run),
        patch.dict(os.environ),
    ):
        return guard.check(tree), calls


def test_check_passes_when_idf_py_changes_nothing(tmp_path: Path) -> None:
    tree = _make_tree(tmp_path)
    problems, calls = _run_check(tree)
    assert problems == []
    sdkconfig = f"SDKCONFIG={tree / 'sdkconfig.dev'}"
    assert calls == [
        ["/py", str(Path("/idf/tools/idf.py")), "-D", sdkconfig, "reconfigure"],
        ["/py", str(Path("/idf/tools/idf.py")), "-D", sdkconfig, "build"],
    ]


def test_check_pins_source_date_epoch(tmp_path: Path) -> None:
    """ESP-IDF's openthread bakes the configure time into its compile flags."""
    envs: list[dict[str, str]] = []
    _run_check(_make_tree(tmp_path), envs=envs)
    assert [env.get("SOURCE_DATE_EPOCH") for env in envs] == ["0", "0"]


def test_check_reports_changed_files_and_rebuilt_outputs(tmp_path: Path) -> None:
    tree = _make_tree(tmp_path)
    build = tree / "build"

    def drift(cmd: list[str]) -> None:
        if cmd[-1] == "reconfigure":
            (build / "CMakeCache.txt").write_text("changed")
            return
        # Compacted log. The re-logged bootloader byproduct and a stamp are
        # not work; a new object mtime is.
        (build / ".ninja_log").write_text(
            "# ninja log v7\n3\t4\t20\tesp-idf/a.obj\t0\n"
            "5\t6\t30\tbootloader/bootloader.bin\t0\n"
            "5\t6\t30\tbootloader-stamp\t0\n"
        )
        # The bootloader sub-build is judged by its own log.
        (build / "bootloader" / ".ninja_log").write_text(
            "# ninja log v7\n1\t2\t40\tbootloader.elf\t0\n"
        )

    problems, _ = _run_check(tree, drift)
    assert problems == [
        "idf.py changed build/CMakeCache.txt",
        "idf.py rebuilt esp-idf/a.obj",
        "idf.py rebuilt bootloader.elf",
    ]


@pytest.mark.parametrize(
    ("after_build", "problem"),
    [
        (lambda log: log.unlink(), "missing build/.ninja_log"),
        (
            lambda log: log.write_text("# ninja log v7\n"),
            "no build entries parsed from build/.ninja_log",
        ),
        (
            lambda log: log.write_text("# ninja log v7\n1\t2\t10\tesp-idf/b.obj\t0\n"),
            "idf.py dropped esp-idf/a.obj from build/.ninja_log",
        ),
    ],
    ids=["log-removed", "log-emptied", "entry-dropped"],
)
def test_check_reports_a_log_idf_py_left_unusable(
    tmp_path: Path, after_build: Callable[[Path], None], problem: str
) -> None:
    """The comparison side gets the same log checks as the baseline."""
    tree = _make_tree(tmp_path)
    log = tree / guard.TOP_NINJA_LOG

    def run(cmd: list[str]) -> None:
        if cmd[-1] == "build":
            after_build(log)

    problems, _ = _run_check(tree, run)
    assert problem in problems


def test_check_stops_when_idf_py_fails(tmp_path: Path) -> None:
    problems, calls = _run_check(_make_tree(tmp_path), rc=2)
    assert problems == ["idf.py reconfigure failed:\nout\nerr\n"]
    assert len(calls) == 1


@pytest.mark.parametrize("remove", ["build/build.ninja", "build/dev.bin", *ALL_LOGS])
def test_check_fails_when_an_input_is_missing(tmp_path: Path, remove: str) -> None:
    """A moved or renamed output must not compare as unchanged."""
    tree = _make_tree(tmp_path)
    (tree / remove).unlink()
    problems, calls = _run_check(tree)
    assert problems == [f"missing {remove}"]
    assert calls == []


@pytest.mark.parametrize(
    ("esphome_rcs", "problem"),
    [
        ((3, 0), "ESPHome's CMake configure failed with exit code 3"),
        ((0, 4), "ESPHome's ninja build failed with exit code 4"),
    ],
    ids=["configure", "build"],
)
def test_check_stops_when_the_esphome_baseline_fails(
    tmp_path: Path, esphome_rcs: tuple[int, int], problem: str
) -> None:
    """The baseline is ESPHome's own reconfigure and build."""
    problems, calls = _run_check(_make_tree(tmp_path), esphome_rcs=esphome_rcs)
    assert problems == [problem]
    assert calls == []


@pytest.mark.parametrize("log", ALL_LOGS)
def test_check_fails_when_a_ninja_log_has_no_entries(tmp_path: Path, log: str) -> None:
    """A log format change must not leave the rebuild check with nothing to compare."""
    tree = _make_tree(tmp_path)
    (tree / log).write_text("# ninja log v99\n1 2 3\n")
    problems, calls = _run_check(tree)
    assert problems == [f"no build entries parsed from {log}"]
    assert calls == []


def test_main_rejects_a_path_that_is_not_a_tree(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    tree = _make_tree(tmp_path / "a")
    stale = tmp_path / "stale"
    with (
        patch.object(sys, "argv", ["check", str(tree), str(stale)]),
        patch.object(guard, "check", return_value=[]) as mock_check,
    ):
        assert guard.main() == 1
    assert f"{stale}: not a configured native ESP-IDF build tree" in (
        capsys.readouterr().out
    )
    mock_check.assert_not_called()


def test_check_resets_the_skip_memo_per_tree(tmp_path: Path) -> None:
    """A second tree must not inherit the first tree's memoized mode."""
    tree = _make_tree(tmp_path)
    CORE.skip_bootloader = True
    toolchain._cache().skip_bootloader = True  # leftover from a prior tree
    problems, _ = _run_check(tree)
    assert problems == []
    assert toolchain._skip_bootloader() is False


def test_check_accepts_a_skip_bootloader_tree(tmp_path: Path) -> None:
    """No bootloader bin or sub-build is the skip shape, not missing input."""
    tree = _make_tree(tmp_path, skip_bootloader=True)
    problems, calls = _run_check(tree)
    assert problems == []
    assert len(calls) == 2
    # The baseline reconfigure must not flip the tree's mode.
    assert CORE.skip_bootloader is True


def test_check_flags_an_ineffective_override(tmp_path: Path) -> None:
    """A skip-mode tree that still built a bootloader must fail CI."""
    tree = _make_tree(tmp_path, skip_bootloader=True)
    (tree / guard.BOOTLOADER_BIN).parent.mkdir(parents=True)
    (tree / guard.BOOTLOADER_BIN).write_bytes(b"x")
    problems, _ = _run_check(tree)
    assert problems == [guard.OVERRIDE_INEFFECTIVE]


def test_check_requires_the_sub_log_on_a_stock_tree(tmp_path: Path) -> None:
    """The mode comes from the define, so a vanished sub-build stays an error."""
    tree = _make_tree(tmp_path)
    (tree / BOOTLOADER_LOG).unlink()
    problems, calls = _run_check(tree)
    assert problems == [f"missing {BOOTLOADER_LOG}"]
    assert calls == []


def test_check_fails_loudly_when_the_idf_macro_changed(tmp_path: Path) -> None:
    """An IDF bump that rewrites the overridden macro must fail CI."""
    tree = _make_tree(tmp_path)
    problems, calls = _run_check(tree, macro_matches=False)
    assert problems == [guard.MACRO_CHANGED]
    assert calls == []


def test_main_without_build_trees(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    with (
        patch.object(sys, "argv", ["check"]),
        patch.object(guard, "REPO_ROOT", tmp_path),
    ):
        assert guard.main() == 1
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
