"""Platform-neutral helpers for ninja-driven native builds."""

from __future__ import annotations

import json
import logging
import os
from pathlib import Path
import re
import shutil
import subprocess

from esphome.core import EsphomeError
from esphome.framework_helpers import strip_win_long_path_prefix, tool_version_runs
from esphome.helpers import write_file_if_changed

_LOGGER = logging.getLogger(__name__)


def _ninja_runs(binary: str) -> bool:
    """Whether the ninja found on PATH actually runs (see tool_version_runs)."""
    return tool_version_runs(
        binary,
        "Ignoring ninja at %s because it failed to run; "
        "falling back to the bundled wheel",
    )


# Compile rule names the generators emit; ninja's compdb tool is asked for
# exactly these, so a renamed rule fails the build instead of stranding idedata
COMPILE_RULES = ("c", "cxx", "aspp", "asm")


def absolute_tool(tool: str | Path) -> str:
    """A tool path that still resolves from the build directory.

    ``shutil.which`` returns a relative path for a relative PATH entry, and
    ninja runs the commands from ``.pioenvs/<name>``. Symlinks are kept:
    ccache's compiler links depend on the name they are called by.
    """
    return strip_win_long_path_prefix(str(Path(tool).absolute()))


def find_ninja() -> Path:
    """Locate the ninja binary: a runnable PATH hit first, else the ninja
    PyPI wheel."""
    if binary := shutil.which("ninja"):
        binary = absolute_tool(binary)
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
    """Escape a path or token for a ninja file.

    ninja has no escape for ``|`` or a line break in a path, so those fail
    here by name instead of producing a build file ninja misreads.
    """
    text = str(value)
    if bad := next((c for c in "|\r\n" if c in text), None):
        raise EsphomeError(
            f"Path {text!r} contains {bad!r}, which a ninja build file cannot "
            "express; rename the file or directory"
        )
    return text.replace("$", "$$").replace(":", "$:").replace(" ", "$ ")


def quote_arg(tok: str) -> str:
    """Quote with the CreateProcess argv rule (as ``subprocess.list2cmdline``):
    backslash runs double only before a quote. Windows-only; ``$`` must
    already be doubled for ninja.
    """
    quoted = re.sub(r'(\\*)"', lambda m: m.group(1) * 2 + '\\"', tok)
    quoted = re.sub(r"(\\+)\Z", lambda m: m.group(1) * 2, quoted)
    return f'"{quoted}"'


# Force-quote any token containing a character outside the shlex.quote-style
# safe set: ninja hands POSIX commands to /bin/sh -c, so bare (, ;, <, *, `
# and friends would be re-parsed as shell syntax.
_NEEDS_QUOTE = re.compile(r"[^\w@%+=:,./-]")


def shell_token(tok: str, force: bool = False) -> str:
    """Re-quote a lexed token for the platform shell; ``force`` always quotes.

    Single quotes on POSIX (/bin/sh), the argv rule on Windows
    (CreateProcess). ``$`` is doubled first because ninja expands it before
    the command reaches the shell.
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


def refresh_compile_commands(
    ninja_path: Path, build_dir: Path, env: dict[str, str], ninja_changed: bool
) -> None:
    """Regenerate the compile DB (a pure function of build.ninja) when stale.

    Freshness rides a stamp: the DB itself is written through
    write_file_if_changed (its mtime feeds the idedata cache), so a
    regeneration with identical content would stay "stale" forever. An
    interrupted previous run may have rewritten the manifest without
    regenerating the DB, hence the mtime comparison.
    """
    compdb = build_dir / "compile_commands.json"
    compdb_stamp = build_dir / ".compile_commands.stamp"
    ninja_file = build_dir / "build.ninja"
    if (
        ninja_changed
        or not compdb.is_file()
        or not compdb_stamp.is_file()
        or compdb_stamp.stat().st_mtime < ninja_file.stat().st_mtime
    ):
        write_compile_commands(ninja_path, build_dir, env)
        compdb_stamp.touch()


def write_compile_commands(
    ninja_path: Path, build_dir: Path, env: dict[str, str]
) -> None:
    compdb = build_dir / "compile_commands.json"
    result = subprocess.run(
        [str(ninja_path), "-C", str(build_dir), "-t", "compdb", *COMPILE_RULES],
        env=env,
        capture_output=True,
        text=True,
        check=False,
        close_fds=False,
    )
    if result.returncode != 0:
        # Drop any stale database so consumers (IDE integration, clang-tidy,
        # the memory analyzer) can't silently read outdated data
        compdb.unlink(missing_ok=True)
        raise EsphomeError(f"Could not generate compile_commands.json: {result.stderr}")
    try:
        entries = json.loads(result.stdout)
    except ValueError as err:
        compdb.unlink(missing_ok=True)
        raise EsphomeError(
            f"ninja produced an unparsable compile database: {err} "
            f"(output starts {result.stdout[:120]!r})"
        ) from err
    if not entries:
        # compdb exits 0 with [] for unknown rule names; a renamed compile
        # rule must fail the build, not silently strand every consumer
        compdb.unlink(missing_ok=True)
        raise EsphomeError(
            "ninja produced an empty compile database; the generator's rule "
            "names no longer match"
        )
    # write_file_if_changed keeps the mtime stable on no-op builds so the
    # idedata cache stays valid
    write_file_if_changed(compdb, result.stdout)
