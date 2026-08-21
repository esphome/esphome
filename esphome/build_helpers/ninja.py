"""Platform-neutral helpers for ninja-driven native builds."""

from __future__ import annotations

import logging
import os
from pathlib import Path
import re
import shutil
import subprocess

from esphome.core import EsphomeError
from esphome.framework_helpers import strip_win_long_path_prefix

_LOGGER = logging.getLogger(__name__)


def _ninja_runs(binary: str) -> bool:
    """Whether the ninja found on PATH actually runs.

    Same rationale as the ccache probe: ``shutil.which`` proves existence,
    not runnability (stale shims, broken wrappers).
    """
    try:
        subprocess.run(
            [binary, "--version"],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=15,
            # Repo-wide convention (posix_spawn fast path)
            close_fds=False,
        )
    except (OSError, subprocess.SubprocessError):
        _LOGGER.warning(
            "Ignoring ninja at %s because it failed to run; "
            "falling back to the bundled wheel",
            binary,
        )
        return False
    return True


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
    quote itself is escaped. POSIX sh parses the result identically for
    backslashes and quotes. ``$`` must already be doubled for ninja.
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
    same argv element SCons would pass under PlatformIO. After ninja
    un-doubles ``$$``, sh still applies every expansion double quotes allow
    (``$VAR``, ``$(...)``, backticks) while CreateProcess passes them
    literally; SCons on POSIX spawns without a shell, so this is a known,
    deliberate divergence for tokens carrying those characters.
    """
    tok = tok.replace("$", "$$")  # ninja would expand a bare $ to nothing
    if force or not tok or _NEEDS_QUOTE.search(tok):
        # An empty token must become "" or it vanishes from the argv
        return quote_arg(tok)
    return tok


def quote_path(value: Path | str) -> str:
    """Force-quote a path for the ninja command line (shell/CreateProcess)."""
    return shell_token(str(value), force=True)
