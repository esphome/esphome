"""Tests for the parallel-upload progress display."""

from __future__ import annotations

import io
import re

import pytest

from esphome.helpers import ProgressBar
from esphome.multi_progress import (
    ENV_FORCE_PROGRESS,
    JobProgress,
    MultiProgressDisplay,
    ProgressFrameFilter,
)


def _frame(percent: int, header: str = "Uploading") -> str:
    """Build a frame exactly as ProgressBar writes it."""
    block = round(60 * percent / 100)
    status = "Done...\r\n" if percent >= 100 else ""
    return f"\r{header}: [{'=' * block + ' ' * (60 - block)}] {percent}% {status}"


def _run_filter(chunks: list[str]) -> tuple[str, list[tuple[str, int]]]:
    seen: list[tuple[str, int]] = []
    frames = ProgressFrameFilter(lambda header, pct: seen.append((header, pct)))
    text = "".join(frames.feed(chunk) for chunk in chunks) + frames.flush()
    return text, seen


def test_frame_helper_matches_progress_bar(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """The frames these tests feed in are the ones ProgressBar really writes."""
    monkeypatch.setenv(ENV_FORCE_PROGRESS, "1")
    bar = ProgressBar("Uploading")
    assert bar.enabled
    bar.update(0.5)
    bar.update(1)

    assert capsys.readouterr().err == _frame(50) + _frame(100)


def test_progress_bar_stays_off_in_a_pipe_by_default(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv(ENV_FORCE_PROGRESS, raising=False)
    assert not ProgressBar("Uploading", stream=io.StringIO()).enabled


def test_filter_strips_frames_and_reports_progress() -> None:
    stream = (
        "INFO Preparing for upload took 0.01 seconds\n"
        + _frame(0)
        + _frame(42)
        + _frame(100)
        + "INFO Upload took 5.01 seconds\n"
    )

    text, seen = _run_filter([stream])

    assert text == (
        "INFO Preparing for upload took 0.01 seconds\nINFO Upload took 5.01 seconds\n"
    )
    assert seen == [("Uploading", 0), ("Uploading", 42), ("Uploading", 100)]


@pytest.mark.parametrize("size", [1, 2, 3, 7, 64])
def test_filter_handles_frames_split_across_chunks(size: int) -> None:
    stream = "before\n" + _frame(9) + _frame(10) + _frame(100) + "after\n"
    chunks = [stream[i : i + size] for i in range(0, len(stream), size)]

    text, seen = _run_filter(chunks)

    assert text == "before\nafter\n"
    assert [pct for _, pct in seen] == [9, 10, 100]


@pytest.mark.parametrize("size", [1, 5, 1000])
def test_filter_drops_newline_that_closed_an_unfinished_bar(size: int) -> None:
    """ProgressBar.done() ends a partial bar's line; no blank line is left."""
    stream = "before\n" + _frame(60) + "\nERROR timed out\n\nafter\n"
    chunks = [stream[i : i + size] for i in range(0, len(stream), size)]

    text, seen = _run_filter(chunks)

    assert text == "before\nERROR timed out\n\nafter\n"
    assert seen == [("Uploading", 60)]


@pytest.mark.parametrize("size", [1, 4, 1000])
def test_filter_handles_windows_line_endings(size: int) -> None:
    """A Windows child's text-mode stream writes CRLF for every newline."""
    stream = (
        "before\n" + _frame(50) + _frame(100) + "after\n" + _frame(60) + "\nend\n"
    ).replace("\n", "\r\n")
    chunks = [stream[i : i + size] for i in range(0, len(stream), size)]

    text, seen = _run_filter(chunks)

    assert text == "before\r\nafter\r\nend\r\n"
    assert [pct for _, pct in seen] == [50, 100, 60]


def test_filter_keeps_text_that_follows_an_unfinished_bar() -> None:
    """Only a newline is dropped after a partial bar, not other text."""
    text, seen = _run_filter([_frame(60), "ERROR timed out\n"])

    assert text == "ERROR timed out\n"
    assert seen == [("Uploading", 60)]


def test_filter_leaves_other_carriage_returns_alone() -> None:
    stream = "line one\r\nspinner |\rspinner /\rdone\n"

    text, seen = _run_filter([stream[:15], stream[15:]])

    assert text == stream
    assert seen == []


def test_filter_flush_returns_unfinished_tail() -> None:
    text, seen = _run_filter(["log\n", "\rUploading: [==="])

    assert text == "log\n\rUploading: [==="
    assert seen == []


def _screen(raw: str) -> list[str]:
    """Replay cursor-up / clear-to-end / newline output into screen lines."""
    lines = [""]
    row = 0
    for match in re.finditer(r"\x1b\[(\d+)A\r\x1b\[J|\x1b\[[\d;]*m|\n|[^\x1b\n]+", raw):
        token = match.group(0)
        if match.group(1):
            row -= int(match.group(1))
            assert row >= 0
            del lines[row + 1 :]
            lines[row] = ""
        elif token == "\n":
            row += 1
            if row == len(lines):
                lines.append("")
        elif not token.startswith("\x1b"):
            lines[row] += token
    assert lines[-1] == ""
    return lines[:-1]


def test_display_shows_running_jobs_then_retires_them() -> None:
    stream = io.StringIO()
    display = MultiProgressDisplay(3, stream, name_width=5)

    display.start("one")
    display.start("two")
    display.update("one", "Uploading", 50)
    screen = _screen(stream.getvalue())
    assert [line.split()[0] for line in screen] == ["one", "two", "Total"]
    assert "Uploading" in screen[0]
    assert screen[0].endswith(" 50%")
    assert "starting" in screen[1]
    assert screen[2].startswith(" Total (0/3)")
    assert screen[2].endswith(" 16%")

    # A finished job moves above the block; the next one takes a row
    display.finish("one", True)
    display.start("three")
    display.update("two", "Uploading", 30)
    display.finish("two", False)
    screen = _screen(stream.getvalue())
    assert [line.split()[0] for line in screen] == ["one", "two", "three", "Total"]
    assert screen[0].endswith("100%")
    assert "FAILED" in screen[1]
    assert screen[1].endswith(" 30%")
    assert screen[3].startswith(" Total (2/3)")

    display.finish("three", True)
    screen = _screen(stream.getvalue())
    assert [line.split()[0] for line in screen] == ["one", "two", "three", "Total"]
    assert screen[3].startswith(" Total (3/3)")
    assert screen[3].endswith("100%")


def test_display_lines_fit_the_terminal(monkeypatch: pytest.MonkeyPatch) -> None:
    """A line that wrapped would break the cursor movement."""
    monkeypatch.setenv("COLUMNS", "50")
    stream = io.StringIO()
    display = MultiProgressDisplay(1, stream, name_width=40)

    display.start("a-very-long-configuration-name-that-cannot-fit")
    display.update("a-very-long-configuration-name-that-cannot-fit", "Uploading", 5)

    screen = _screen(stream.getvalue())
    assert all(len(line) < 50 for line in screen)
    assert "..." in screen[0]


def test_display_ignores_unknown_jobs() -> None:
    stream = io.StringIO()
    display = MultiProgressDisplay(1, stream)

    display.update("ghost", "Uploading", 5)
    display.finish("ghost", True)

    assert stream.getvalue() == ""


class _RecordingDisplay:
    """Stands in for MultiProgressDisplay, keeping the calls it was given."""

    def __init__(self) -> None:
        self.calls: list[tuple] = []

    def start(self, name: str) -> None:
        self.calls.append(("start", name))

    def update(self, name: str, status: str, percent: int | None = None) -> None:
        self.calls.append(("update", status, percent))

    def finish(self, name: str, ok: bool) -> None:
        self.calls.append(("finish", ok))


@pytest.mark.parametrize("size", [1, 9, 10_000])
def test_job_progress_reports_stages_and_retries(size: int) -> None:
    """The status follows the child's log, and a retry stays visible."""
    # Lines as esphome.espota2 logs them for a first attempt that drops
    stream = (
        "INFO Host porch-light.local not in cache, will need to resolve\n"
        "INFO Connecting to 10.10.10.36 port 8266...\n"
        "INFO Connected to 10.10.10.36\n"
        + _frame(0)
        + _frame(40)
        + "\nWARNING Error receiving acknowledge chunk OK: timed out\n"
        "INFO Retrying in 5 seconds (attempt 2 of 3)...\n"
        "INFO Connecting to 10.10.10.36 port 8266...\n"
        + _frame(0)
        + _frame(100)
        + "INFO OTA successful\n"
    )
    display = _RecordingDisplay()
    job = JobProgress(display, "porch-light")

    text = "".join(job.feed(stream[i : i + size]) for i in range(0, len(stream), size))
    text += job.flush()
    job.finish(True)

    assert "Uploading" not in text
    assert "INFO Retrying in 5 seconds (attempt 2 of 3)...\n" in text
    assert display.calls == [
        ("start", "porch-light"),
        ("update", "resolving", None),
        ("update", "connecting", None),
        ("update", "Uploading", 0),
        ("update", "Uploading", 40),
        # The retry resets the bar and then stays as the status
        ("update", "retry 2/3", 0),
        ("update", "retry 2/3", 0),
        ("update", "retry 2/3", 100),
        ("finish", True),
    ]


def test_job_progress_ignores_failed_connect_line() -> None:
    """Only the attempt itself counts as connecting, not its failure."""
    display = _RecordingDisplay()
    job = JobProgress(display, "a")

    job.feed("WARNING Connecting to 10.0.0.1 port 3232 failed: timed out\n")

    assert display.calls == [("start", "a")]


def test_display_update_without_percent_keeps_the_bar() -> None:
    stream = io.StringIO()
    display = MultiProgressDisplay(1, stream, name_width=5)
    display.start("one")
    display.update("one", "Uploading", 40)

    display.update("one", "retry 2/3")

    screen = _screen(stream.getvalue())
    assert "retry 2/3" in screen[0]
    assert screen[0].endswith(" 40%")


def test_display_does_not_redraw_when_nothing_changed() -> None:
    stream = io.StringIO()
    display = MultiProgressDisplay(1, stream, name_width=5)
    display.start("one")
    display.update("one", "Uploading", 40)
    before = stream.getvalue()

    display.update("one", "Uploading", 40)
    display.update("one", "Uploading")

    assert stream.getvalue() == before
