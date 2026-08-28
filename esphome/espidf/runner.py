r"""Subprocess entry point for running ``idf.py`` with stdio wrapping.

Invoked as ``python runner.py <script_path> [script args...]``.

Wraps ``sys.stdout`` and ``sys.stderr`` with a ``_FilteringTTYStream``
shim so that:

1. ``isatty()`` unconditionally returns True. CMake, Ninja, and idf.py's
   own progress-bar code all check ``stream.isatty()`` to decide between
   TTY-format output (``\\r`` cursor moves, ANSI colors, fancy progress
   bars) and a plain fallback. With the wrapper in place they always
   emit TTY format, even when our real stdout is a pipe to the parent
   process (e.g. running under the Home Assistant dashboard add-on).
   Downstream consumers — local terminals and the HA dashboard log
   viewer — render the TTY control sequences correctly.

2. ``FILTER_IDF_LINES`` is applied inside the shim's ``write()`` so
   noisy idf.py output is dropped before it leaves this subprocess.
   Filtering is skipped when ``-v`` / ``--verbose`` appears in argv so
   verbose mode still shows everything.

ESP-IDF runs under its own Python virtual environment which does not
have the ``esphome`` package installed, so the runner is intentionally
self-contained: no imports from ``esphome`` at all. The line-filtering
wrapper is inlined below rather than imported from
``esphome.util.RedirectText`` for that reason.
"""

import sys

# Regex patterns matched against each line of idf.py / CMake / Ninja
# output. Lines that match are dropped before reaching the parent
# process. Patterns are anchored at the start of the line (the shim
# uses ``re.match``). Disabled when the user passes ``-v`` /
# ``--verbose`` to ``esphome compile``.
FILTER_IDF_LINES: list[str] = [
    # idf.py's "how to flash" block at the end of a successful build.
    # ESPHome handles flashing itself, so these instructions just clutter
    # the output.
    r"Project build complete\.",
    r" idf\.py ",
    r" python -m esptool ",
    r"or$",
    r"or from the ",
    # CMake dumps the full list of IDF component paths on one giant line.
    # It's purely informational and bloats the log.
    r"-- Component paths:",
    # CMake lists every linker script it adds (dozens of lines) and the
    # complete flat list of IDF components on one giant line. Neither
    # has diagnostic value for end users.
    r"-- Adding linker script ",
    r"-- Components:",
    # IDF component manager notices: emitted on first build (no lock),
    # once per stubbed dependency, plus the final "Processing N
    # dependencies" enumeration. Patterns allow a leading run of dots
    # because the component manager prints progress dots on the same
    # line, so a NOTICE often arrives prefixed with ".NOTICE:" or
    # "...........NOTICE:".
    r"\.*NOTICE: ",
    # ``idf.py size`` prefaces its table with a centered banner; the
    # per-region table below already makes the structure obvious.
    r"\s*Memory Type Usage Summary",
    # Prefix match for esp-idf-size's trailing "Note:" paragraph (no
    # upstream flag suppresses it).
    r"Note: The reported total sizes may be smaller than those in the",
    # Drop the blank line rich emits after the note so the build log
    # doesn't end with an orphan gap before ESPHome's own status lines.
    r"\s*$",
    # ESP-IDF shells out to ``git rev-parse`` to embed a commit hash;
    # esphome-libs strips ``.git`` from the tarball so those probes fail
    # noisily without affecting the build.
    r"-- git rev-parse returned ",
    r"fatal: not a git repository",
    r"Stopping at filesystem boundary",
]


def main() -> int:
    # ---- sys.path fix-up ---------------------------------------------------
    #
    # When Python runs this file as ``python runner.py``, it prepends the
    # script's directory — ``<site-packages>/esphome/espidf/`` — to
    # ``sys.path[0]``. That directory is part of the esphome package whose
    # sibling ``types.py`` (in ``esphome/``) collides with stdlib ``types``.
    # Any subsequent import that transitively touches ``types`` (``runpy``,
    # ``pathlib``, ``functools``, ``typing``, ...) could resolve the wrong
    # module. Drop the entry pre-emptively. ``sys`` is a built-in so
    # importing it at module level earlier did not trigger the shadow.
    if sys.path and sys.path[0]:
        sys.path.pop(0)
    # ---- end sys.path fix-up -----------------------------------------------

    import contextlib
    import os
    from pathlib import Path
    import re
    import runpy

    # Patch ``os.get_terminal_size`` to return a fallback size instead
    # of raising ``OSError`` when the underlying fd isn't a real
    # terminal.
    #
    # idf.py's ``fit_text_in_terminal`` (in ``idf_py_actions/tools.py``)
    # unconditionally calls ``os.get_terminal_size()`` to format ninja
    # progress lines. When that raises ``[Errno 25] Inappropriate
    # ioctl for device`` on our pipe-backed stdout, idf.py catches the
    # exception as ``EnvironmentError`` and silently exits its stdout
    # reader coroutine — dropping all ninja build output from that
    # point on. Returning a valid value keeps the coroutine alive so
    # progress and error lines continue to flow through to the parent
    # process.
    #
    # Honour the ``COLUMNS`` / ``LINES`` env vars if the caller set
    # them explicitly. Otherwise fall back to ``(0, 0)``, which
    # ``fit_text_in_terminal`` treats as "unknown width, don't
    # truncate" (see the ``if not terminal_width: return out`` guard).
    # Downstream log viewers (local terminals, the HA dashboard) wrap
    # or scroll long lines themselves, so we'd rather emit the full
    # file path than have idf.py elide its middle.
    _orig_get_terminal_size = os.get_terminal_size

    def _get_terminal_size_fallback(fd: int = 1) -> os.terminal_size:
        try:
            return _orig_get_terminal_size(fd)
        except OSError:
            try:
                columns = int(os.environ.get("COLUMNS", "0"))
            except ValueError:
                columns = 0
            try:
                lines = int(os.environ.get("LINES", "0"))
            except ValueError:
                lines = 0
            return os.terminal_size((columns, lines))

    os.get_terminal_size = _get_terminal_size_fallback  # type: ignore[assignment]

    # Strip ANSI escape sequences before comparing a line against the filter
    # patterns, so colorized lines still match plain-text patterns.
    ansi_escape = re.compile(r"\033[@-_][0-?]*[ -/]*[@-~]")

    class _FilteringTTYStream:
        r"""Minimal stdout/stderr wrapper.

        * ``isatty()`` unconditionally returns True, tricking downstream
          code into emitting TTY-format output.
        * Input is split with ``str.splitlines(keepends=True)``, which
          breaks on more than ``\\n`` and ``\\r``; form feed and a few
          other control characters count too. Any piece whose
          ANSI-stripped, right-stripped form matches one of
          ``filter_lines`` is dropped.
        * Only the final piece can still be waiting for more text, so
          that one is held until a ``\\n`` or ``\\r`` arrives. A piece
          that ended on one of the other breaks goes out as it is.

        Mirrors the matching semantics of ``esphome.util.RedirectText``
        so filter patterns behave identically in both the PlatformIO
        and IDF runner paths.
        """

        def __init__(self, stream, filter_lines: list[str] | None) -> None:
            self._stream = stream
            if filter_lines:
                combined = r"|".join(r"(?:" + p + r")" for p in filter_lines)
                self._filter_pattern: re.Pattern[str] | None = re.compile(combined)
            else:
                self._filter_pattern = None
            self._line_buffer = ""

        def __getattr__(self, name: str):
            # Hide ``buffer`` so consumers that use either
            # ``getattr(stream, 'buffer', None)`` or
            # ``hasattr(stream, 'buffer')`` see this as a text-only stream
            # and skip writing raw bytes (which would bypass the filter).
            if name == "buffer":
                raise AttributeError(name)
            return getattr(self._stream, name)

        def isatty(self) -> bool:
            return True

        def flush(self) -> None:
            self._stream.flush()

        def _emit(self, line: str) -> None:
            if self._filter_pattern is not None:
                stripped = ansi_escape.sub("", line).rstrip()
                if self._filter_pattern.match(stripped) is not None:
                    return
            self._stream.write(line)

        def drain(self) -> None:
            """Write out a held-back line that never got its terminator.

            idf.py and CMake do not always end their last line with a
            newline, and a build that dies part way through can stop mid
            line. Without this the user is left staring at a build that
            ended with no explanation.
            """
            if not self._line_buffer:
                return
            line, self._line_buffer = self._line_buffer, ""
            try:
                # Add the terminator the line never got, so whatever ESPHome
                # prints next does not run onto the same line.
                self._emit(line + "\n")
                self._stream.flush()
            except (OSError, ValueError) as err:
                # We are called from cleanup, so raising would replace the
                # build's real exit code. Saying so must not raise either:
                # under the dashboard our stdout and stderr are the same
                # pipe, so whatever broke the write has most likely broken
                # the report, and ``sys.__stderr__`` is None on some
                # interpreters. Carry the line along; it is usually the
                # message saying why the build failed.
                if (real_stderr := sys.__stderr__) is not None:
                    with contextlib.suppress(OSError, ValueError):
                        print(
                            f"Could not write out remaining output ({err}): {line}",
                            file=real_stderr,
                        )

        def write(self, data) -> int:
            # Text streams normally hand us ``str``; decode in case
            # somebody writes bytes directly.
            if not isinstance(data, str):
                data = data.decode(errors="replace")

            if self._filter_pattern is None:
                # Nothing to match against, so no need to wait for a full line.
                self._emit(data)
            else:
                lines = (self._line_buffer + data).splitlines(keepends=True)
                # Every piece but the last ends with something
                # ``str.splitlines`` treats as a break, so only the last one
                # can still be waiting for more text. Hold that one, write
                # out the rest.
                #
                # Some of those breaks are not line endings to us, a form
                # feed for one, so a piece can go out without ending in a
                # newline. That beats what we did before, which was to stop
                # at the first such piece and drop every complete line
                # behind it.
                if lines and not lines[-1].endswith(("\n", "\r")):
                    self._line_buffer = lines.pop()
                else:
                    self._line_buffer = ""
                for line in lines:
                    self._emit(line)

            # We tell idf.py it is talking to a terminal, so it sends progress
            # bars and cursor moves. Our own stdout is usually a pipe, which is
            # block buffered, so without this the build looks frozen until
            # 8 KiB of output piles up.
            self._stream.flush()
            return len(data)

    if len(sys.argv) < 2:
        print(
            "usage: runner.py <script_path> [args...]",
            file=sys.stderr,
        )
        return 2

    script_path = sys.argv[1]

    # Mirror the platformio runner behaviour: verbose mode disables the
    # line filter so all output reaches the user.
    is_verbose = any(arg in ("-v", "--verbose") for arg in sys.argv[2:])
    filter_lines = None if is_verbose else FILTER_IDF_LINES or None

    stdout_shim = sys.stdout = _FilteringTTYStream(sys.stdout, filter_lines)  # type: ignore[assignment]
    stderr_shim = sys.stderr = _FilteringTTYStream(sys.stderr, filter_lines)  # type: ignore[assignment]

    # Shift argv so the target script sees its own path as argv[0] and
    # its own arguments starting at argv[1]. runpy.run_path does not
    # modify sys.argv itself.
    sys.argv = [script_path] + sys.argv[2:]

    # Emulate Python's default behaviour of prepending the script's
    # directory to sys.path[0] when running ``python script.py``.
    # runpy.run_path does not do this automatically, but idf.py relies
    # on it to import its sibling modules (python_version_checker,
    # idf_py_actions, ...).
    script_dir = str(Path(script_path).resolve().parent)
    if script_dir not in sys.path:
        sys.path.insert(0, script_dir)

    # If idf.py calls sys.exit(), SystemExit propagates out of run_path
    # and carries the exit code back to our caller. For normal returns,
    # fall through and exit with 0. Either way the streams get a chance to
    # release a last line that never got its terminator. Drain the shims we
    # made rather than sys.stdout, which the script is free to replace, and
    # report instead of raising so cleanup cannot bury the real exit code.
    try:
        runpy.run_path(script_path, run_name="__main__")
    finally:
        # Drain stderr from a finally so a surprise from the first one cannot
        # strand the second.
        try:
            stdout_shim.drain()
        finally:
            stderr_shim.drain()
    return 0


if __name__ == "__main__":
    sys.exit(main())
