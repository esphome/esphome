"""Tests for esphome.parallel_runner."""

import os
from pathlib import Path
import sys
from unittest.mock import MagicMock

import pytest

from esphome.parallel_runner import ChildProcesses, run_captured, run_parallel


def child_script(body: str) -> list[str]:
    """Build a command that runs a small Python snippet as the child."""
    return [sys.executable, "-c", body]


@pytest.mark.parametrize("live", [False, True])
def test_run_config_captured_does_not_start_after_stop(
    tmp_path: Path, live: bool
) -> None:
    """A worker that reaches its child after Ctrl-C must not start it."""
    marker = tmp_path / "started"
    script = "import sys, pathlib; pathlib.Path(sys.argv[1]).touch()"
    children = ChildProcesses()
    children.stop()
    display = MagicMock() if live else None

    rc, output = run_captured(
        [*child_script(script), str(marker)], dict(os.environ), children, display, "a"
    )

    assert rc != 0
    assert "stopped" in output
    assert not marker.exists()
    if display is not None:
        display.finish.assert_called_once_with("a", False)


def test_child_processes_stop_terminates_running_children() -> None:
    """stop() ends children that were started before it."""
    children = ChildProcesses()
    proc = children.start(
        child_script("import time; print('up', flush=True); time.sleep(60)"),
        dict(os.environ),
    )
    assert proc is not None
    with proc:
        assert proc.stdout.readline().strip() == b"up"
        children.stop()
        assert proc.wait(timeout=30) != 0
    children.finished(proc)


def test_run_parallel_reports_each_result(tmp_path: Path) -> None:
    """Every file's exit code and output reach the callback."""
    files = [tmp_path / "good.yaml", tmp_path / "bad.yaml"]
    script = "import sys; print('ran ' + sys.argv[1]); sys.exit(sys.argv[1] == 'bad')"
    results: dict[str | Path, tuple[int, str]] = {}
    env = dict(os.environ)

    run_parallel(
        files,
        lambda f: [*child_script(script), Path(f).stem],
        env,
        2,
        lambda f, rc, output: results.update({f: (rc, output)}),
    )

    assert results == {files[0]: (0, "ran good\n"), files[1]: (1, "ran bad\n")}
    # The caller's environment is left alone
    assert env == dict(os.environ)
