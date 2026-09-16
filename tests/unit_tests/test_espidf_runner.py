"""Tests for esphome.espidf.runner."""

from __future__ import annotations

import io
import os
from pathlib import Path
import subprocess
import sys
import threading

import pytest

from esphome.espidf import runner

# A flushing runner delivers the first line in well under a second; this is
# only ever waited out when the shim has gone back to buffering, so keep it
# just long enough to cover interpreter startup on a loaded CI machine.
FIRST_LINE_TIMEOUT = 10.0


def _prepare_main(
    monkeypatch: pytest.MonkeyPatch, probe: Path, *args: str
) -> tuple[io.BytesIO, io.TextIOWrapper]:
    """Point ``runner.main()`` at *probe* with a buffered fake stdout.

    ``main`` rewrites ``sys.path``, ``sys.argv``, both std streams and
    ``os.get_terminal_size``; every one of those is monkeypatched so it is
    put back afterwards. The fake stdout is block buffered like a pipe, so
    the caller can tell whether the shim flushed. The wrapper comes back with
    the buffer because dropping it would close the buffer underneath us.
    """
    buf = io.BytesIO()
    stream = io.TextIOWrapper(buf, encoding="utf-8", newline="\n", line_buffering=False)

    monkeypatch.setattr(sys, "path", list(sys.path))
    monkeypatch.setattr(sys, "argv", ["runner.py", str(probe), *args])
    monkeypatch.setattr(sys, "stdout", stream)
    monkeypatch.setattr(sys, "stderr", stream)
    monkeypatch.setattr(os, "get_terminal_size", os.get_terminal_size)

    return buf, stream


def _run_main(
    monkeypatch: pytest.MonkeyPatch, probe: Path, *args: str
) -> tuple[io.BytesIO, io.TextIOWrapper]:
    """Run ``runner.main()`` against *probe* and expect a clean exit."""
    buf, stream = _prepare_main(monkeypatch, probe, *args)
    assert runner.main() == 0
    return buf, stream


def test_main_filters_noise_and_flushes_each_write(
    monkeypatch: pytest.MonkeyPatch, fixture_path: Path
) -> None:
    """Useful lines reach the stream right away; noisy ones are dropped."""
    buf, _stream = _run_main(
        monkeypatch, fixture_path / "espidf" / "filtering_probe.py"
    )

    # Read before any flush of our own: the shim has to have flushed.
    output = buf.getvalue().decode("utf-8")

    assert "Compiling main.cpp\n" in output
    assert "[2/9] Building C object\n" in output
    # Matched by FILTER_IDF_LINES, so they never leave the runner.
    assert "Project build complete." not in output
    assert "-- Component paths:" not in output
    # Held back until the end because no terminator arrived.
    assert output.endswith("still going\n")


def test_main_keeps_output_after_a_form_feed(
    monkeypatch: pytest.MonkeyPatch, fixture_path: Path
) -> None:
    """A form feed is text, not a line break, so nothing after it is lost."""
    buf, _stream = _run_main(monkeypatch, fixture_path / "espidf" / "formfeed_probe.py")

    assert buf.getvalue().decode("utf-8") == (
        "Compiling main.cpp\npage one\x0cpage two\n[2/9] Building C object\n"
    )


def test_main_drains_a_partial_line_when_the_build_dies(
    monkeypatch: pytest.MonkeyPatch, fixture_path: Path
) -> None:
    """A build that stops mid line must still show that line.

    This is the whole point of draining: the message explaining why the
    build failed is exactly the one most likely to arrive without a
    trailing newline.
    """
    buf, _stream = _prepare_main(
        monkeypatch, fixture_path / "espidf" / "crashing_probe.py"
    )

    with pytest.raises(SystemExit) as excinfo:
        runner.main()

    assert excinfo.value.code == 2
    assert buf.getvalue().decode("utf-8") == "FATAL: ld returned 1 exit status\n"


def test_main_reports_rather_than_raises_when_draining_fails(
    monkeypatch: pytest.MonkeyPatch,
    fixture_path: Path,
    capfd: pytest.CaptureFixture[str],
) -> None:
    """A stream that closed under us must not crash the runner's cleanup.

    The drain runs from a ``finally``, so an exception there would replace
    whatever exit code the build was carrying back.
    """
    _prepare_main(monkeypatch, fixture_path / "espidf" / "closing_probe.py")

    assert runner.main() == 0
    reported = capfd.readouterr().err
    assert "Could not write out remaining output" in reported
    # The held line has to come along; the stream it was meant for is gone.
    assert "partial before close" in reported


def test_main_survives_a_drain_failure_with_nowhere_to_report_it(
    monkeypatch: pytest.MonkeyPatch, fixture_path: Path
) -> None:
    """With no real stderr to report to, cleanup still must not raise.

    ``sys.__stderr__`` is None on some interpreters, and ``print(file=None)``
    falls back to ``sys.stdout``, which here is the shim wrapping the stream
    that just failed.
    """
    monkeypatch.setattr(sys, "__stderr__", None)
    _prepare_main(monkeypatch, fixture_path / "espidf" / "closing_probe.py")

    assert runner.main() == 0


def test_main_still_filters_a_drained_partial_line(
    monkeypatch: pytest.MonkeyPatch, fixture_path: Path
) -> None:
    """Releasing a held line does not smuggle noise past the filter."""
    buf, _stream = _run_main(
        monkeypatch, fixture_path / "espidf" / "partial_noise_probe.py"
    )

    assert buf.getvalue().decode("utf-8") == "Compiling main.cpp\n"


def test_main_keeps_everything_in_verbose_mode(
    monkeypatch: pytest.MonkeyPatch, fixture_path: Path
) -> None:
    """``-v`` turns the filter off so the noisy lines survive."""
    buf, _stream = _run_main(
        monkeypatch, fixture_path / "espidf" / "filtering_probe.py", "-v"
    )

    output = buf.getvalue().decode("utf-8")

    assert "Project build complete.\n" in output
    assert "-- Component paths: /a /b /c\n" in output
    # With no filter there is no line buffering, so the partial line goes
    # straight through as well.
    assert output.endswith("still going")


def test_runner_streams_output_before_the_build_finishes(
    fixture_path: Path, probe_env: dict[str, str]
) -> None:
    """The runner must flush, or a dashboard build looks frozen.

    ``toolchain.py`` spawns the runner as a plain script with no ``-u``, and
    hands it a pipe when esphome itself is running under the dashboard. A
    pipe is block buffered, so without a flush in the shim's ``write()`` the
    output sits in the child until 8 KiB piles up or the build ends.
    """
    runner_py = Path(runner.__file__)
    probe = fixture_path / "espidf" / "streaming_probe.py"

    with subprocess.Popen(
        [sys.executable, str(runner_py), str(probe)],
        stdout=subprocess.PIPE,
        # Keep stderr: if the runner dies on startup, its traceback is the
        # only clue about why no line showed up.
        stderr=subprocess.PIPE,
        env=probe_env,
        text=True,
    ) as proc:
        assert proc.stdout is not None
        assert proc.stderr is not None
        first_line: list[str] = []
        reader = threading.Thread(
            target=lambda: first_line.append(proc.stdout.readline()), daemon=True
        )
        try:
            reader.start()
            reader.join(FIRST_LINE_TIMEOUT)
            still_running = proc.poll() is None

            # The probe sleeps for a minute after writing, so reaching us at
            # all means the line was flushed rather than released at exit.
            assert first_line == ["Compiling main.cpp\n"], (
                f"runner stderr: {'' if still_running else proc.stderr.read()}"
            )
            assert still_running
        finally:
            proc.kill()
            proc.wait()
            # Join before leaving the block, so the reader is done rather than
            # racing ``Popen`` closing the pipe under it.
            reader.join(1.0)
