"""Platform-neutral helpers for ninja-driven native builds."""

from __future__ import annotations

import logging
import os
from pathlib import Path
import re
import shutil

from esphome.core import EsphomeError
from esphome.framework_helpers import strip_win_long_path_prefix, tool_version_runs

_LOGGER = logging.getLogger(__name__)


def _ninja_runs(binary: str) -> bool:
    """Whether the ninja found on PATH actually runs (see tool_version_runs)."""
    return tool_version_runs(
        binary,
        "Ignoring ninja at %s because it failed to run; "
        "falling back to the bundled wheel",
    )


def find_ninja() -> Path:
    """Locate the ninja binary: a runnable PATH hit first, else the ninja
    PyPI wheel.

    The wheel is a requirements.txt dependency, so pip has already
    integrity-checked it; no download logic is needed here.
    """
    if binary := shutil.which("ninja"):
        binary = strip_win_long_path_prefix(binary)
        if _ninja_runs(binary):
            return Path(binary)
    import_error: ImportError | None = None
    try:
        import ninja
    except ImportError as err:
        import_error = err
        wheel_binary = None
    else:
        wheel_binary = Path(ninja.BIN_DIR) / (
            "ninja.exe" if os.name == "nt" else "ninja"
        )
    if wheel_binary is None or not wheel_binary.is_file():
        raise EsphomeError(
            "ninja not found on PATH or in the ninja package; reinstall the "
            "esphome Python environment"
        ) from import_error
    return wheel_binary


def escape(value: Path | str) -> str:
    """Escape a path or token for a ninja file."""
    return str(value).replace("$", "$$").replace(":", "$:").replace(" ", "$ ")


def quote_arg(tok: str) -> str:
    """Wrap a token in double quotes with the Windows argv rule.

    Same escaping rule as ``subprocess.list2cmdline``: a backslash run
    doubles only immediately before a quote (or the closing quote), and the
    quote itself is escaped. CreateProcess-only; POSIX sh collapses
    backslash runs inside double quotes, so shell_token single-quotes
    there instead. ``$`` must already be doubled for ninja.
    """
    quoted = re.sub(r'(\\*)"', lambda m: m.group(1) * 2 + '\\"', tok)
    quoted = re.sub(r"(\\+)\Z", lambda m: m.group(1) * 2, quoted)
    return f'"{quoted}"'


# Force-quote any token containing a character outside the shlex.quote-style
# safe set: ninja hands POSIX commands to /bin/sh -c, so bare (, ;, <, *, `
# and friends would be re-parsed as shell syntax.
_NEEDS_QUOTE = re.compile(r"[^\w@%+=:,./-]")


def shell_token(tok: str, force: bool = False) -> str:
    """Quote a lexed token only when needed; ``force`` always quotes.

    Lexing strips the quoting a user wrote (``-DX="a b"`` becomes the single
    token ``-DX=a b``); re-quote on the way out so the compiler receives the
    same argv element SCons would pass under PlatformIO. Ninja hands POSIX
    commands to ``/bin/sh -c`` and Windows commands to CreateProcess, so the
    quoting style is chosen per platform: single quotes on POSIX (sh expands
    nothing inside them, matching SCons's no-shell spawn) and the argv rule
    on Windows. ``$`` is doubled first in either case because ninja expands
    ``$`` before the command reaches the shell.
    """
    tok = tok.replace("$", "$$")  # ninja would expand a bare $ to nothing
    if not (force or not tok or _NEEDS_QUOTE.search(tok)):
        return tok
    # An empty token must become '' / "" or it vanishes from the argv
    if os.name == "nt":
        return quote_arg(tok)
    # shlex.quote's rule; inlined because the $-doubled token must not be
    # re-examined for safe characters
    return "'" + tok.replace("'", "'\"'\"'") + "'"


def quote_path(value: Path | str) -> str:
    """Force-quote a path for the ninja command line (shell/CreateProcess)."""
    return shell_token(str(value), force=True)
