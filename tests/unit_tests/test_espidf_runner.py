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


def _run_main(
    monkeypatch: pytest.MonkeyPatch, probe: Path, *args: str
) -> tuple[io.BytesIO, io.TextIOWrapper]:
    """Run ``runner.main()`` in-process against a buffered fake stdout.

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
    # Held back because no terminator arrived.
    assert "still going" not in output


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
