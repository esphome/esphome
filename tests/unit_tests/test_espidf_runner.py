"""Tests for esphome.espidf.runner."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import threading

# The runner tells idf.py it is on a terminal, so a build produces plenty of
# output; if the first line does not arrive well inside this window the shim
# is buffering it.
FIRST_LINE_TIMEOUT = 30.0


def test_runner_streams_output_before_the_build_finishes(
    fixture_path: Path, probe_env: dict[str, str]
) -> None:
    """The runner must flush, or a dashboard build looks frozen.

    ``toolchain.py`` spawns the runner as a plain script with no ``-u``, and
    hands it a pipe when esphome itself is running under the dashboard. A
    pipe is block buffered, so without a flush in the shim's ``write()`` the
    output sits in the child until 8 KiB piles up or the build ends.
    """
    runner = Path(__file__).parent.parent.parent / "esphome" / "espidf" / "runner.py"
    probe = fixture_path / "espidf" / "streaming_probe.py"

    proc = subprocess.Popen(
        [sys.executable, str(runner), str(probe)],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        env=probe_env,
        text=True,
    )
    try:
        first_line: list[str] = []
        reader = threading.Thread(
            target=lambda: first_line.append(proc.stdout.readline())
        )
        reader.daemon = True
        reader.start()
        reader.join(FIRST_LINE_TIMEOUT)

        # The probe sleeps for a minute after writing, so reaching us at all
        # means the line was flushed rather than released at exit.
        assert first_line == ["Compiling main.cpp\n"]
        assert proc.poll() is None
    finally:
        proc.kill()
        proc.wait()
