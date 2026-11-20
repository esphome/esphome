"""Run one child process per configuration, several at a time.

Each child's output is captured so the children do not interleave on the
terminal. This module is only imported when a command is run with more than
one job.
"""

import codecs
from collections.abc import Callable
import concurrent.futures
from pathlib import Path
import subprocess
import threading
from typing import TextIO

from esphome.multi_progress import ENV_FORCE_PROGRESS, JobProgress, MultiProgressDisplay


class ChildProcesses:
    """The children of a parallel run, so that all of them can be stopped.

    Starting a child and stopping them all take the same lock: a child is
    either started before the stop and terminated by it, or not started.
    """

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._running: set[subprocess.Popen[bytes]] = set()
        self._stopped = False

    def start(
        self, cmd: list[str], env: dict[str, str]
    ) -> subprocess.Popen[bytes] | None:
        """Start a child with its output captured; None once stopped."""
        with self._lock:
            if self._stopped:
                return None
            # The caller enters it; it has to be registered before the lock
            # is released.
            proc = subprocess.Popen(  # pylint: disable=consider-using-with
                cmd,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                # Unbuffered, so a read returns what the child has written so
                # far instead of waiting for a full buffer.
                bufsize=0,
                env=env,
            )
            self._running.add(proc)
            return proc

    def finished(self, proc: subprocess.Popen[bytes]) -> None:
        """Forget a child that has closed its output."""
        with self._lock:
            self._running.discard(proc)

    def stop(self) -> None:
        """Terminate the running children and refuse to start any more."""
        with self._lock:
            self._stopped = True
            for proc in self._running:
                proc.terminate()


def run_captured(
    cmd: list[str],
    env: dict[str, str],
    children: ChildProcesses,
    display: MultiProgressDisplay | None = None,
    name: str = "",
) -> tuple[int, str]:
    """Run one child process to completion, returning its exit code and output.

    stdout and stderr are merged and captured so parallel children do not
    interleave on the terminal. stdin is closed so a child that would prompt
    fails instead of blocking the whole batch.

    With a display, the child's progress-bar frames are taken out of the
    returned output and shown live on the display's row for ``name``, along
    with the stage (resolving, connecting, retrying) its log lines report.
    """
    progress = JobProgress(display, name) if display is not None else None

    try:
        proc = children.start(cmd, env)
    except OSError as err:
        proc = None
        failure = f"Running command failed: {err}\n"
    else:
        failure = "Not started: the run was stopped\n"
    if proc is None:
        if progress is not None:
            progress.finish(False)
        return 1, failure

    decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
    output = []
    with proc:
        try:
            while chunk := proc.stdout.read(4096):
                text = decoder.decode(chunk)
                output.append(progress.feed(text) if progress is not None else text)
        finally:
            children.finished(proc)
    rc = proc.returncode
    output.append(decoder.decode(b"", final=True))
    if progress is not None:
        output.append(progress.flush())
        progress.finish(rc == 0)
    # Children write the platform's line endings; print() adds them back.
    return rc, "".join(output).replace("\r\n", "\n")


def run_parallel(
    files: list[str | Path],
    command_builder: Callable[[str | Path], list[str]],
    env: dict[str, str],
    jobs: int,
    on_result: Callable[[str | Path, int, str], None],
    terminal: TextIO | None = None,
) -> None:
    """Run a command for each file, up to ``jobs`` of them at once.

    Args:
        files: Configuration files to process.
        command_builder: Takes a file path and returns its command list.
        env: Environment for the children.
        jobs: Number of children to run at once.
        on_result: Called in this thread with each file, its child's exit code
            and its captured output.
        terminal: Where to draw live progress bars. With one, ``on_result`` is
            called in the order of ``files`` once every child has finished;
            without, it is called as each child finishes.
    """
    # A piped stdout is block-buffered while stderr is not; unbuffered
    # keeps each child's log lines and prints in the order they happened.
    env = {**env, "PYTHONUNBUFFERED": "1"}
    # Shown on the bars: file names are enough unless two of them clash
    names = {f: Path(f).stem for f in files}
    if len(set(names.values())) != len(names):
        names = {f: str(f) for f in files}
    display = None
    if terminal is not None:
        # The children write to a pipe, where the bar is normally off
        env[ENV_FORCE_PROGRESS] = "1"
        display = MultiProgressDisplay(
            len(files), terminal, max(len(n) for n in names.values())
        )

    children = ChildProcesses()
    results: dict[str | Path, tuple[int, str]] = {}
    executor = concurrent.futures.ThreadPoolExecutor(max_workers=min(jobs, len(files)))
    try:
        futures = {
            executor.submit(
                run_captured, command_builder(f), env, children, display, names[f]
            ): f
            for f in files
        }
        for future in concurrent.futures.as_completed(futures):
            f = futures[future]
            if terminal is None:
                on_result(f, *future.result())
            else:
                results[f] = future.result()
    except BaseException:
        # Ctrl-C or an unexpected error: don't start queued configs and
        # don't leave running children behind.
        executor.shutdown(wait=False, cancel_futures=True)
        children.stop()
        raise
    executor.shutdown()
    if terminal is not None:
        terminal.write("\n")
        terminal.flush()
        for f in files:
            on_result(f, *results[f])
