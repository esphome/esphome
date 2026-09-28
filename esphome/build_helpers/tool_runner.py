"""Run a native build tool (cmake, ninja) and relay its output.

Output is read from a pipe so it can be filtered here: a child that inherits
our stdout writes straight to the file descriptor, past any Python wrapper.
"""

from __future__ import annotations

import codecs
from contextlib import suppress
import logging
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import Any, TextIO

from esphome.util import ANSI_ESCAPE, RedirectText, shlex_quote

_LOGGER = logging.getLogger(__name__)

# Windows code page identifier for UTF-8, as used by ``chcp 65001``.
UTF8_CODEPAGE = 65001

# Same pattern idf.py uses to spot ninja status lines (``is_progression``).
_PROGRESS = re.compile(r"^\[\d+/\d+\]|.*\(\d+ \%\)$")
_READ_SIZE = 65536


def _get_kernel32() -> Any | None:
    """Return the Windows kernel32 module, or None on any other platform."""
    if sys.platform != "win32":
        return None
    import ctypes

    return ctypes.windll.kernel32


class Utf8Console:
    """Keep an attached Windows console on UTF-8 while a build tool runs.

    esp_idf_size draws its table with Unicode box characters, and CMake
    re-decodes a child's output with the console code page, which garbles the
    table on any page but UTF-8. A console already on UTF-8 is left alone so
    an overlapping build never records UTF-8 as the page to go back to.
    """

    def __init__(self, kernel32: Any | None) -> None:
        self._kernel32 = kernel32
        self._codepages: tuple[int, int] | None = None

    def __enter__(self) -> None:
        kernel32 = self._kernel32
        if kernel32 is None:
            return
        old_in = kernel32.GetConsoleCP()
        old_out = kernel32.GetConsoleOutputCP()
        # Both calls return 0 when no console is attached.
        if not old_in or not old_out:
            return
        if old_in == UTF8_CODEPAGE and old_out == UTF8_CODEPAGE:
            return
        # Record first so a switch that fails part way is still undone.
        self._codepages = (old_in, old_out)
        kernel32.SetConsoleCP(UTF8_CODEPAGE)
        kernel32.SetConsoleOutputCP(UTF8_CODEPAGE)

    def __exit__(self, *exc_info: object) -> None:
        if self._codepages is None:
            return
        old_in, old_out = self._codepages
        self._codepages = None
        self._kernel32.SetConsoleCP(old_in)
        self._kernel32.SetConsoleOutputCP(old_out)


def _fit_terminal(text: str) -> str:
    """Elide the middle of ``text`` to fit the terminal, as idf.py does.

    A width of 0 (a pipe, the dashboard) leaves the text whole.
    """
    width = shutil.get_terminal_size((0, 0)).columns
    if not width:
        return text
    if width <= 3:
        return "." * width
    if len(text) >= width:
        keep = (width - 3) // 2
        return f"{text[:keep]}...{text[len(text) - keep :]}"
    return text


class ToolOutput(RedirectText):
    """RedirectText that can collapse ninja status lines into one line.

    With ``progress`` each ``[n/m]`` line overwrites the previous one, the
    way idf.py shows a build. Needs ``filter_lines``: RedirectText only
    splits the stream into lines when it has something to match.
    """

    def __init__(
        self, out: TextIO, filter_lines: list[str] | None, progress: bool
    ) -> None:
        super().__init__(out, filter_lines=filter_lines)
        self._progress = progress
        self._on_progress_line = False

    def _emit_line(self, line: str) -> None:
        if self._progress and _PROGRESS.match(line):
            if not self._is_filtered(ANSI_ESCAPE.sub("", line).rstrip()):
                text = _fit_terminal(line.strip("\r\n"))
                self._write_color_replace(f"\r{text}\x1b[K")
                self._on_progress_line = True
            return
        self._end_progress_line()
        super()._emit_line(line)

    def _end_progress_line(self) -> None:
        if self._on_progress_line:
            self._on_progress_line = False
            self._write_color_replace(os.linesep)

    def drain(self) -> None:
        super().drain()
        # Called from cleanup, so a broken stream must not hide the exit code.
        with suppress(OSError, ValueError):
            self._end_progress_line()
            self._out.flush()


def run_build_tool(
    cmd: list[str],
    *,
    cwd: Path,
    env: dict[str, str],
    filter_lines: list[str] | None = None,
    progress: bool = False,
) -> int:
    """Run ``cmd`` and relay stdout and stderr, merged, to our stdout.

    Returns the exit code.
    """
    _LOGGER.debug("Running: %s", " ".join(shlex_quote(arg) for arg in cmd))
    _LOGGER.debug("  in directory: %s", cwd)
    output = ToolOutput(sys.stdout, filter_lines, progress)
    decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
    with (
        Utf8Console(_get_kernel32()),
        subprocess.Popen(
            cmd,
            cwd=cwd,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            close_fds=False,
        ) as proc,
    ):
        try:
            # read1 returns as soon as anything is available, so output
            # streams while the tool runs.
            while chunk := proc.stdout.read1(_READ_SIZE):
                output.write(decoder.decode(chunk))
            if tail := decoder.decode(b"", final=True):
                output.write(tail)
        finally:
            output.drain()
    return proc.returncode
