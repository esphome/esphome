"""Tests for esphome.build_helpers.tool_runner."""

# pylint: disable=protected-access

import io
import os
from pathlib import Path
import sys
import threading
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from esphome.build_helpers import tool_runner
from esphome.build_helpers.tool_runner import (
    UTF8_CODEPAGE,
    ToolOutput,
    Utf8Console,
    run_build_tool,
)
from esphome.core import CORE

FILTER = [r"-- Component paths:", r"\s*$"]


def _child(code: str) -> list[str]:
    return [sys.executable, "-c", code]


def _run(
    capsys: pytest.CaptureFixture[str],
    tmp_path: Path,
    code: str,
    **kwargs: object,
) -> tuple[int, str]:
    rc = run_build_tool(_child(code), cwd=tmp_path, env=dict(os.environ), **kwargs)
    # The child's print() ends lines with \r\n on Windows.
    return rc, capsys.readouterr().out.replace("\r\n", "\n")


def test_run_build_tool_filters_and_returns_exit_code(
    capsys: pytest.CaptureFixture[str], tmp_path: Path
) -> None:
    rc, out = _run(
        capsys,
        tmp_path,
        "import sys\n"
        "print('-- Component paths: /a /b')\n"
        "print('')\n"
        "print('Compiling main.cpp')\n"
        "sys.exit(3)",
        filter_lines=FILTER,
    )
    assert rc == 3
    assert out == "Compiling main.cpp\n"


def test_run_build_tool_passes_everything_without_filter(
    capsys: pytest.CaptureFixture[str], tmp_path: Path
) -> None:
    rc, out = _run(capsys, tmp_path, "print('-- Component paths: /a')\nprint('')")
    assert rc == 0
    assert out == "-- Component paths: /a\n\n"


def test_run_build_tool_merges_stderr_and_runs_in_cwd(
    capsys: pytest.CaptureFixture[str], tmp_path: Path
) -> None:
    _, out = _run(
        capsys,
        tmp_path,
        "import os, sys\nprint(os.getcwd(), flush=True)\nprint('oops', file=sys.stderr)",
        filter_lines=FILTER,
    )
    assert out.splitlines() == [os.path.realpath(tmp_path), "oops"]


def test_run_build_tool_drains_a_partial_line(
    capsys: pytest.CaptureFixture[str], tmp_path: Path
) -> None:
    """A tool that dies mid line still shows that line, terminated."""
    rc, out = _run(
        capsys,
        tmp_path,
        "import sys\nsys.stdout.write('ld returned 1 exit status')\nsys.exit(1)",
        filter_lines=FILTER,
    )
    assert rc == 1
    assert out == "ld returned 1 exit status\n"


def test_run_build_tool_replaces_invalid_utf8(
    capsys: pytest.CaptureFixture[str], tmp_path: Path
) -> None:
    _, out = _run(
        capsys,
        tmp_path,
        # A multi-byte character split across writes must survive too.
        "import sys\nb = sys.stdout.buffer\n"
        "b.write(b'\\xc3'); b.flush(); b.write(b'\\xa9 ok\\n\\xff bad\\n')",
        filter_lines=FILTER,
    )
    assert out == "é ok\n� bad\n"


def test_run_build_tool_streams_before_the_tool_exits(tmp_path: Path) -> None:
    """Output must reach the user while the tool runs, not when it ends."""
    seen = threading.Event()
    lines: list[str] = []

    class _Out(io.StringIO):
        def write(self, s: str) -> int:
            lines.append(s)
            if "first" in s:
                seen.set()
            return len(s)

    release = tmp_path / "release"
    code = (
        "import os, time\nprint('first', flush=True)\n"
        f"while not os.path.exists({str(release)!r}): time.sleep(0.02)\n"
    )
    with patch.object(tool_runner.sys, "stdout", _Out()):
        thread = threading.Thread(
            target=run_build_tool,
            args=(_child(code),),
            kwargs={"cwd": tmp_path, "env": dict(os.environ), "filter_lines": FILTER},
        )
        thread.start()
        try:
            assert seen.wait(30)
        finally:
            release.touch()
            thread.join(30)
    assert "first" in "".join(lines)


def _tool_output(progress: bool = True) -> tuple[ToolOutput, io.StringIO]:
    out = io.StringIO()
    return ToolOutput(out, FILTER, progress), out


def test_tool_output_collapses_progress_lines() -> None:
    """Ninja status lines overwrite each other, as idf.py shows them."""
    output, out = _tool_output()
    output.write("[1/2] Building a.o\n[2/2] Linking app\nwarning: x\n")
    assert out.getvalue() == (
        "\r[1/2] Building a.o\x1b[K\r[2/2] Linking app\x1b[K"
        + os.linesep
        + "warning: x\n"
    )


def test_tool_output_drain_ends_a_progress_line() -> None:
    output, out = _tool_output()
    output.write("[1/1] Linking app\n")
    output.drain()
    assert out.getvalue() == "\r[1/1] Linking app\x1b[K" + os.linesep
    # Nothing is pending any more.
    output.drain()
    assert out.getvalue().count(os.linesep) == 1


def test_tool_output_drain_survives_a_broken_stream() -> None:
    output, out = _tool_output()
    output.write("[1/1] Linking app\n")
    out.close()
    output.drain()


def test_tool_output_filters_a_matching_progress_line() -> None:
    out = io.StringIO()
    output = ToolOutput(out, [r"\[1/2\]"], True)
    output.write("[1/2] hidden\n[2/2] shown\n")
    assert out.getvalue() == "\r[2/2] shown\x1b[K"


def test_tool_output_without_progress_keeps_status_lines() -> None:
    output, out = _tool_output(progress=False)
    output.write("[1/2] Building a.o\n")
    assert out.getvalue() == "[1/2] Building a.o\n"


def test_tool_output_escapes_colors_for_the_dashboard() -> None:
    CORE.dashboard = True
    output, out = _tool_output()
    output.write("[1/1] \x1b[1mLinking\x1b[0m\n")
    assert "\x1b" not in out.getvalue()
    assert "\\033[K" in out.getvalue()


@pytest.mark.parametrize(
    ("width", "text", "expected"),
    [
        (0, "x" * 50, "x" * 50),
        (3, "abcdef", "..."),
        (20, "short", "short"),
        (11, "abcdefghijklmnop", "abcd...mnop"),
    ],
    ids=["unknown", "tiny", "fits", "elided"],
)
def test_fit_terminal(width: int, text: str, expected: str) -> None:
    with patch.object(
        tool_runner.shutil,
        "get_terminal_size",
        return_value=os.terminal_size((width, 24)),
    ):
        assert tool_runner._fit_terminal(text) == expected


class _FakeKernel32:
    """Stand-in for the Windows kernel32 console code page calls."""

    def __init__(self, input_cp: int, output_cp: int) -> None:
        self.input_cp = input_cp
        self.output_cp = output_cp
        self.calls: list[tuple[str, int]] = []

    def GetConsoleCP(self) -> int:  # noqa: N802
        return self.input_cp

    def GetConsoleOutputCP(self) -> int:  # noqa: N802
        return self.output_cp

    def SetConsoleCP(self, codepage: int) -> int:  # noqa: N802
        self.calls.append(("SetConsoleCP", codepage))
        self.input_cp = codepage
        return 1

    def SetConsoleOutputCP(self, codepage: int) -> int:  # noqa: N802
        self.calls.append(("SetConsoleOutputCP", codepage))
        self.output_cp = codepage
        return 1


def test_run_build_tool_switches_the_console_to_utf8(
    capsys: pytest.CaptureFixture[str], tmp_path: Path
) -> None:
    """An attached console runs the tool on UTF-8 and is then put back."""
    kernel32 = _FakeKernel32(850, 850)
    with patch.object(tool_runner, "_get_kernel32", return_value=kernel32):
        _run(capsys, tmp_path, "print('x')")
    assert kernel32.calls == [
        ("SetConsoleCP", UTF8_CODEPAGE),
        ("SetConsoleOutputCP", UTF8_CODEPAGE),
        ("SetConsoleCP", 850),
        ("SetConsoleOutputCP", 850),
    ]


def test_utf8_console_restores_after_an_error() -> None:
    kernel32 = _FakeKernel32(437, 437)
    with pytest.raises(RuntimeError), Utf8Console(kernel32):
        raise RuntimeError
    assert (kernel32.input_cp, kernel32.output_cp) == (437, 437)


def test_utf8_console_restores_when_the_switch_fails_part_way() -> None:
    kernel32 = _FakeKernel32(850, 850)

    def _refuse(codepage: int) -> int:
        kernel32.calls.append(("SetConsoleOutputCP", codepage))
        return 0

    kernel32.SetConsoleOutputCP = _refuse  # type: ignore[method-assign]
    with Utf8Console(kernel32):
        pass
    assert kernel32.input_cp == 850
    assert kernel32.calls[-2:] == [("SetConsoleCP", 850), ("SetConsoleOutputCP", 850)]


@pytest.mark.parametrize(
    "codepages", [(0, 0), (UTF8_CODEPAGE, UTF8_CODEPAGE)], ids=["none", "utf8"]
)
def test_utf8_console_leaves_the_console_alone(codepages: tuple[int, int]) -> None:
    """No console, or one already on UTF-8 (an overlapping build), is untouched."""
    kernel32 = _FakeKernel32(*codepages)
    with Utf8Console(kernel32):
        pass
    assert kernel32.calls == []


def test_utf8_console_without_kernel32() -> None:
    with Utf8Console(None):
        pass


@pytest.mark.skipif(sys.platform == "win32", reason="kernel32 exists on Windows")
def test_get_kernel32_is_none_off_windows() -> None:
    assert tool_runner._get_kernel32() is None


def test_tool_output_collapses_progress_without_a_filter() -> None:
    """Progress mode splits lines on its own; it does not need a filter."""
    out = io.StringIO()
    output = ToolOutput(out, None, True)
    output.write("[1/1] Linking app\ndone\n")
    assert out.getvalue() == "\r[1/1] Linking app\x1b[K" + os.linesep + "done\n"


def test_run_build_tool_flushes_a_truncated_character(
    capsys: pytest.CaptureFixture[str], tmp_path: Path
) -> None:
    """Output that ends inside a multi-byte character still shows up."""
    _, out = _run(
        capsys,
        tmp_path,
        "import sys\nsys.stdout.buffer.write(b'end \\xc3')",
        filter_lines=FILTER,
    )
    assert out == "end �\n"


def test_get_kernel32_loads_it_on_windows(monkeypatch: pytest.MonkeyPatch) -> None:
    kernel32 = object()
    fake_ctypes = SimpleNamespace(windll=SimpleNamespace(kernel32=kernel32))
    monkeypatch.setattr(tool_runner.sys, "platform", "win32")
    monkeypatch.setitem(sys.modules, "ctypes", fake_ctypes)
    assert tool_runner._get_kernel32() is kernel32


def test_run_build_tool_logs_the_unfiltered_output(
    capsys: pytest.CaptureFixture[str], tmp_path: Path
) -> None:
    """The log gets every line, filtered or not, for idf.py's hint matcher."""
    log = tmp_path / "log" / "ninja_all_output.log"
    _, out = _run(
        capsys,
        tmp_path,
        "print('-- Component paths: /a')\nprint('\\x1b[1merror:\\x1b[0m boom')",
        filter_lines=FILTER,
        log_path=log,
    )
    assert out == "\x1b[1merror:\x1b[0m boom\n"
    assert log.read_text(encoding="utf-8").replace("\r\n", "\n") == (
        "-- Component paths: /a\nerror: boom\n"
    )
