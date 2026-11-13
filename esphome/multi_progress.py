r"""Live progress display for several child processes running at once.

``esphome upload --jobs N`` runs one child per configuration. Each child draws
its usual ``\rUploading: [====    ] NN%`` frames into a pipe; the parent pulls
those frames out of the captured log with :class:`ProgressFrameFilter` and
redraws them as one block of bars, one row per running child, with
:class:`MultiProgressDisplay`.
"""

from collections.abc import Callable
import re
import shutil
import threading
import time
from typing import TextIO

from esphome.helpers import ENV_FORCE_PROGRESS
from esphome.log import AnsiFore, color

__all__ = [
    "ENV_FORCE_PROGRESS",
    "JobProgress",
    "MultiProgressDisplay",
    "ProgressFrameFilter",
]

# One frame as written by esphome.helpers.ProgressBar. The 100% frame ends its
# own line; every other frame leaves the cursor on the line for the next one.
_FRAME_RE = re.compile(
    r"\r(?P<header>[^\r\n\[\]]*): \[[= ]*\] "
    r"(?:(?P<done>100)% Done\.\.\.\r\n|(?P<pct>\d{1,2})% )"
)

# Longest run of output held back while waiting to see whether it is a frame;
# comfortably more than a header plus ProgressBar's 60-column bar.
_MAX_FRAME_LENGTH = 256


class ProgressFrameFilter:
    """Strip progress-bar frames out of a stream of child output.

    Output arrives in arbitrary chunks, so a frame may be split across two
    calls to :meth:`feed`; an unfinished frame is held back until the rest
    arrives.
    """

    def __init__(
        self,
        on_progress: Callable[[str, int], None],
        on_text: Callable[[str], None] | None = None,
    ) -> None:
        """``on_text`` gets the log text too, in order with ``on_progress``."""
        self._on_progress = on_progress
        self._on_text = on_text
        self._pending = ""
        # The last thing read was a frame that left the cursor on its line
        self._mid_line = False

    def feed(self, text: str) -> str:
        """Take the next chunk of output; return the part that is log text."""
        text = self._pending + text
        self._pending = ""
        out = []
        pos = 0
        for match in _FRAME_RE.finditer(text):
            out.append(self._log_text(text[pos : match.start()]))
            self._on_progress(match["header"], int(match["done"] or match["pct"]))
            self._mid_line = match["done"] is None
            pos = match.end()
        tail = text[pos:]
        # A frame starts with a carriage return and has no newline before its
        # end, so one after the last newline may be a frame still arriving.
        start = tail.find("\r", tail.rfind("\n") + 1)
        if start != -1 and len(tail) - start <= _MAX_FRAME_LENGTH:
            tail, self._pending = tail[:start], tail[start:]
        out.append(self._log_text(tail))
        return "".join(out)

    def _log_text(self, text: str) -> str:
        """Drop the newline that only ended an unfinished bar's line."""
        if text and self._mid_line:
            self._mid_line = False
            if text.startswith("\r\n"):
                text = text[2:]
            elif text.startswith("\n"):
                text = text[1:]
        if text and self._on_text is not None:
            self._on_text(text)
        return text

    def flush(self) -> str:
        """Return whatever was held back; call once the stream has ended."""
        text, self._pending = self._pending, ""
        return text


# What a child logs while it works towards (or back to) an upload, and the
# status shown for it. Matched against whole log lines from esphome.espota2
# and esphome.helpers; a line that stops matching only costs the status text.
_RETRY_RE = re.compile(r"Retrying in \d+ seconds \(attempt (\d+) of (\d+)\)")
_LOG_STATUSES = (
    (re.compile(r"not in cache, will need to resolve"), "resolving"),
    (re.compile(r"Connecting to \S+ port \d+\.\.\."), "connecting"),
)


class JobProgress:
    """Turn one child's output into updates of its row on the display.

    Progress-bar frames are removed from the output and drive the bar. Log
    lines pass through untouched, and the ones that mark a stage of the upload
    set the row's status. Once the child reports a retry, the status keeps
    showing the attempt, since a bar that starts over is otherwise unexplained.
    """

    def __init__(self, display: "MultiProgressDisplay", name: str) -> None:
        self._display = display
        self._name = name
        self._frames = ProgressFrameFilter(self._on_frame, self._scan)
        self._line = ""
        self._retry: str | None = None
        display.start(name)

    def _on_frame(self, header: str, percent: int) -> None:
        self._display.update(self._name, self._retry or header, percent)

    def _scan(self, text: str) -> None:
        *lines, self._line = (self._line + text).split("\n")
        for line in lines:
            if match := _RETRY_RE.search(line):
                self._retry = f"retry {match[1]}/{match[2]}"
                # The failed attempt's progress is gone
                self._display.update(self._name, self._retry, 0)
                continue
            if self._retry is not None:
                continue
            for pattern, status in _LOG_STATUSES:
                if pattern.search(line):
                    self._display.update(self._name, status)
                    break

    def feed(self, text: str) -> str:
        """Take the next chunk of output; return the part that is log text."""
        return self._frames.feed(text)

    def flush(self) -> str:
        """Return whatever was held back; call once the stream has ended."""
        return self._frames.flush()

    def finish(self, ok: bool) -> None:
        """Retire the row; call once the child has exited."""
        self._display.finish(self._name, ok)


class _Row:
    def __init__(self, name: str) -> None:
        self.name = name
        self.status = "starting"
        self.percent = 0
        self.started = time.monotonic()


class MultiProgressDisplay:
    """A block of progress bars, one per running job, plus a total.

    A job gets a row when it starts. When it finishes, its row is printed one
    last time above the block, where it scrolls away like any other line, so
    the block is never taller than the number of jobs running at once.

    All methods may be called from any thread.
    """

    _STATUS_WIDTH = 12
    _MIN_BAR_WIDTH = 10
    _MIN_NAME_WIDTH = 8

    def __init__(self, total: int, stream: TextIO, name_width: int = 20) -> None:
        self._total = total
        self._stream = stream
        self._name_width = max(name_width, len(self._total_label(total)))
        self._rows: dict[str, _Row] = {}
        self._finished = 0
        self._drawn = 0
        self._lock = threading.Lock()

    def _total_label(self, finished: int) -> str:
        return f"Total ({finished}/{self._total})"

    def _format(self, name: str, status: str, percent: int) -> str:
        # One column short of the terminal so a full line never wraps;
        # wrapping would break the cursor movement in _redraw.
        columns = shutil.get_terminal_size().columns - 1
        # " name status [bar] 100%"
        fixed = 1 + 1 + self._STATUS_WIDTH + 1 + 2 + 1 + 4
        name_width = self._name_width
        bar_width = columns - fixed - name_width
        if bar_width < self._MIN_BAR_WIDTH:
            name_width = max(
                self._MIN_NAME_WIDTH, name_width - (self._MIN_BAR_WIDTH - bar_width)
            )
            bar_width = self._MIN_BAR_WIDTH
        if len(name) > name_width:
            name = name[: name_width - 3] + "..."
        filled = bar_width * percent // 100
        progress = "#" * filled + "-" * (bar_width - filled)
        status = status[: self._STATUS_WIDTH]
        return (
            f" {name:<{name_width}} {status:<{self._STATUS_WIDTH}} "
            f"[{progress}] {percent:>3}%"
        )

    def _redraw(self, permanent: str | None = None) -> None:
        """Replace the block on screen, first printing ``permanent`` above it."""
        out = []
        if self._drawn:
            # Up to the block's first line, then clear to the end of the screen
            out.append(f"\x1b[{self._drawn}A\r\x1b[J")
        if permanent is not None:
            out.append(permanent + "\n")
        lines = [
            self._format(row.name, row.status, row.percent)
            for row in self._rows.values()
        ]
        if self._finished < self._total:
            done = self._finished * 100 + sum(r.percent for r in self._rows.values())
            lines.append(
                self._format(self._total_label(self._finished), "", done // self._total)
            )
        out.extend(line + "\n" for line in lines)
        self._drawn = len(lines)
        self._stream.write("".join(out))
        self._stream.flush()

    def start(self, name: str) -> None:
        """Add a row for a job that has just started."""
        with self._lock:
            self._rows[name] = _Row(name)
            self._redraw()

    def update(self, name: str, status: str, percent: int | None = None) -> None:
        """Set a running job's status text and, if given, how far along it is."""
        with self._lock:
            row = self._rows.get(name)
            if row is None:
                return
            if percent is None:
                percent = row.percent
            if (row.status, row.percent) == (status, percent):
                return
            row.status = status
            row.percent = percent
            self._redraw()

    def finish(self, name: str, ok: bool) -> None:
        """Retire a job's row, leaving its final state above the block."""
        with self._lock:
            row = self._rows.pop(name, None)
            if row is None:
                return
            self._finished += 1
            elapsed = f"{time.monotonic() - row.started:.1f}s"
            if ok:
                line = self._format(name, elapsed, 100)
                line = line.replace("100%", color(AnsiFore.GREEN, "100%"))
            else:
                line = self._format(name, "FAILED", row.percent)
                line = line.replace("FAILED", color(AnsiFore.BOLD_RED, "FAILED"), 1)
            if self._finished == self._total:
                total = self._format(self._total_label(self._finished), "", 100)
                line = f"{line}\n{total}"
            self._redraw(permanent=line)
