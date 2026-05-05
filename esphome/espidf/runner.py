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

    import os
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
        * Input is split on ``\\n`` / ``\\r`` via
          ``str.splitlines(keepends=True)`` and any complete line whose
          ANSI-stripped, right-stripped form matches one of
          ``filter_lines`` is dropped.
        * Incomplete trailing chunks are held in a buffer until a
          terminator arrives.

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
            return getattr(self._stream, name)

        def isatty(self) -> bool:
            return True

        def flush(self) -> None:
            self._stream.flush()

        def write(self, data) -> int:
            # Text streams normally hand us ``str``; decode in case
            # somebody writes bytes directly.
            if not isinstance(data, str):
                data = data.decode(errors="replace")

            if self._filter_pattern is None:
                self._stream.write(data)
                return len(data)

            self._line_buffer += data
            for line in self._line_buffer.splitlines(keepends=True):
                if "\n" not in line and "\r" not in line:
                    # Incomplete — hold until we see a terminator.
                    self._line_buffer = line
                    break
                self._line_buffer = ""

                stripped = ansi_escape.sub("", line).rstrip()
                if self._filter_pattern.match(stripped) is not None:
                    continue
                self._stream.write(line)
            return len(data)

    if len(sys.argv) < 2:
        print(
            "usage: runner.py <script_path> [args...]",
            file=sys.stderr,
        )
        return 2

    script_path = sys.argv[1]

    # Mirror the platformio_runner behaviour: verbose mode disables the
    # line filter so all output reaches the user.
    is_verbose = any(arg in ("-v", "--verbose") for arg in sys.argv[2:])
    filter_lines = None if is_verbose else FILTER_IDF_LINES or None

    sys.stdout = _FilteringTTYStream(sys.stdout, filter_lines)  # type: ignore[assignment]
    sys.stderr = _FilteringTTYStream(sys.stderr, filter_lines)  # type: ignore[assignment]

    # Shift argv so the target script sees its own path as argv[0] and
    # its own arguments starting at argv[1]. runpy.run_path does not
    # modify sys.argv itself.
    sys.argv = [script_path] + sys.argv[2:]

    # Emulate Python's default behaviour of prepending the script's
    # directory to sys.path[0] when running ``python script.py``.
    # runpy.run_path does not do this automatically, but idf.py relies
    # on it to import its sibling modules (python_version_checker,
    # idf_py_actions, ...).
    script_dir = os.path.dirname(os.path.abspath(script_path))
    if script_dir not in sys.path:
        sys.path.insert(0, script_dir)

    # If idf.py calls sys.exit(), SystemExit propagates out of run_path
    # and carries the exit code back to our caller. For normal returns,
    # fall through and exit with 0.
    runpy.run_path(script_path, run_name="__main__")
    return 0


if __name__ == "__main__":
    sys.exit(main())
