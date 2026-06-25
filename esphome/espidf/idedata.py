"""Derive idedata from an ESP-IDF native-toolchain ``compile_commands.json``.

PlatformIO exposes a curated ``pio run -t idedata`` JSON; the native ESP-IDF
toolchain has no such command, but its CMake build emits
``build/compile_commands.json`` (CMAKE_EXPORT_COMPILE_COMMANDS). This module
turns that file into the same fields consumers (IDE integration, clang-tidy)
expect:

    {cxx_path, cxx_flags, defines, includes: {build, toolchain}}
"""

from __future__ import annotations

import json
import logging
import os
from pathlib import Path
import shlex
import subprocess

_LOGGER = logging.getLogger(__name__)

# C++ translation-unit suffixes used to identify ESPHome source files.
_CXX_SUFFIXES = (".cpp", ".cc")
# Suffixes of input/output files that appear bare on the command line (and so
# must not be mistaken for compiler flags).
_INPUT_FILE_SUFFIXES = (*_CXX_SUFFIXES, ".c", ".o", ".S", ".s")
# Path marker identifying an ESPHome source translation unit.
_ESPHOME_SRC_MARKER = "/src/esphome/"


def _is_esphome_src(file: str) -> bool:
    """Whether ``file`` is an ESPHome C++ translation unit.

    ``compile_commands.json`` ``file`` paths use the OS-native separator, so on
    Windows they contain backslashes; normalize to ``/`` before testing the
    marker, otherwise no source matches and the build-include union is empty.
    """
    return _ESPHOME_SRC_MARKER in file.replace("\\", "/") and file.endswith(
        _CXX_SUFFIXES
    )


def _split_command(command: str) -> list[str]:
    r"""Tokenize a compile_commands.json / response-file command string.

    On Windows, tokenize per Windows ``argv`` rules via ``CommandLineToArgvW``.
    ESP-IDF's compile_commands.json there mixes two backslash conventions in one
    string: literal path separators in the compiler path (``C:\Users\...g++.exe``,
    no quote follows) and shell quote-escaping in -D defines (``-DVER=\"1.2.3\"``).
    Only the real Windows parser — where a backslash escapes solely a following
    quote — handles both, and it is the exact tokenizer the compiler is launched
    with. ``shlex`` cannot: POSIX mode eats the path separators, and disabling
    its escape mangles the defines.
    """
    if os.name != "nt":
        return shlex.split(command)

    import ctypes
    from ctypes import wintypes

    # CommandLineToArgvW("") returns the current process name, not []; guard it
    # so an empty response file tokenizes the same as it would via shlex.
    if not command.strip():
        return []

    CommandLineToArgvW = ctypes.windll.shell32.CommandLineToArgvW
    CommandLineToArgvW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_int)]
    CommandLineToArgvW.restype = ctypes.POINTER(wintypes.LPWSTR)
    argc = ctypes.c_int()
    argv = CommandLineToArgvW(command, ctypes.byref(argc))
    if not argv:  # pragma: no cover
        raise ctypes.WinError()
    try:
        return [argv[i] for i in range(argc.value)]
    finally:
        ctypes.windll.kernel32.LocalFree(argv)


def _expand_response_files(tokens: list[str], directory: Path) -> list[str]:
    """Inline any ``@response-file`` arguments (paths relative to ``directory``).

    GCC response files embed flags that must be expanded so GCC-only flags
    inside them (e.g. ``-mlongcalls``) can be filtered downstream; left as
    ``@file`` clang would read them and choke.
    """
    out: list[str] = []
    for tok in tokens:
        if tok.startswith("@"):
            rf = Path(tok[1:])
            if not rf.is_absolute():
                rf = directory / rf
            try:
                out.extend(
                    _expand_response_files(
                        _split_command(rf.read_text(encoding="utf-8")), directory
                    )
                )
                continue
            except OSError as err:
                # Keep the literal token if the file can't be read, but log it
                # so the (otherwise opaque) downstream clang failure is traceable.
                _LOGGER.warning("Could not read response file %s: %s", rf, err)
        out.append(tok)
    return out


def _pick_entry(entries: list[dict]) -> dict:
    """Pick a representative ESPHome C++ translation unit.

    All ESPHome sources share the same component flags/defines, so any one of
    them yields the cxx_path / cxx_flags / defines we need.
    """
    for entry in entries:
        if _is_esphome_src(entry["file"]):
            return entry
    for entry in entries:
        if entry["file"].endswith(_CXX_SUFFIXES):
            return entry
    raise ValueError("no C++ translation unit found in compile_commands.json")


def _parse_entry(entry: dict) -> tuple[str, list[str], list[str], list[str]]:
    """Parse one compile_commands entry -> (cxx_path, defines, includes, cxx_flags)."""
    directory = Path(entry["directory"])
    tokens = _expand_response_files(_split_command(entry["command"]), directory)

    def _include(raw: str) -> str:
        # Include paths in compile_commands are interpreted relative to the
        # entry's ``directory`` (e.g. build-local ``-Iconfig``); resolve them
        # so the cached idedata is usable regardless of the consumer's cwd.
        # Emit forward slashes (``normpath`` yields ``\`` on Windows) so the
        # paths match the absolute, already-forward-slash entries in the JSON.
        raw = raw.strip()
        if raw and not Path(raw).is_absolute():
            raw = os.path.normpath(directory / raw)
        return raw.replace("\\", "/")

    # token0 is the compiler path; the rest of the command already uses forward
    # slashes on Windows, so normalize it too for a consistent idedata file.
    cxx_path = tokens[0].replace("\\", "/")
    defines: list[str] = []
    includes: list[str] = []
    cxx_flags: list[str] = []

    it = iter(tokens[1:])
    for tok in it:
        if tok in ("-c", "-o"):
            next(it, None)  # drop the flag and its argument (input/output)
        elif tok.startswith("-D"):
            # ``.strip()`` handles tokens like ``-D CONFIGURED=1`` (a single
            # quoted arg with a space after -D) that some flags arrive as.
            defines.append(tok[2:].strip() if len(tok) > 2 else next(it, "").strip())
        elif tok.startswith("-I"):
            includes.append(_include(tok[2:] if len(tok) > 2 else next(it, "")))
        elif tok == "-isystem":
            includes.append(_include(next(it, "")))
        elif tok.startswith("-isystem"):
            includes.append(_include(tok[len("-isystem") :]))
        elif tok in ("-MT", "-MF", "-MQ"):
            next(it, None)  # dependency-file flag + its argument
        elif tok.startswith(("-MD", "-MMD", "-MP", "-MM")):
            pass  # dependency-generation flags, no argument
        elif tok.endswith(_INPUT_FILE_SUFFIXES):
            pass  # input/output files
        else:
            cxx_flags.append(tok)
    return cxx_path, defines, includes, cxx_flags


def _get_toolchain_includes(cxx_path: str) -> list[str]:
    """Query the compiler for its builtin ``#include <...>`` search dirs."""
    result = subprocess.run(
        [cxx_path, "-E", "-x", "c++", "-", "-v"],
        input="",
        text=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        check=False,
        close_fds=False,
    )
    includes: list[str] = []
    capture = False
    for line in result.stderr.splitlines():
        if "#include <...> search starts here:" in line:
            capture = True
            continue
        if "End of search list." in line:
            break
        if capture:
            includes.append(line.strip())
    if result.returncode != 0 or not includes:
        raise RuntimeError(
            f"Could not query builtin include dirs from {cxx_path} "
            f"(return code {result.returncode}); stderr:\n{result.stderr.strip()}"
        )
    return includes


def idedata_from_build(compile_commands: Path) -> dict:
    """Parse compile_commands.json into the idedata fields consumers expect.

    A single ESP-IDF compile entry only carries its own component's REQUIRES
    include set, but consumers (clang-tidy) analyze ESPHome headers that
    transitively pull in other components. So take cxx_path / cxx_flags /
    defines from a representative ESPHome TU, but union the include dirs across
    all ESPHome TUs to get a project-wide superset (as PlatformIO's idedata
    provides).
    """
    entries = json.loads(Path(compile_commands).read_text(encoding="utf-8"))
    cxx_path, defines, _, cxx_flags = _parse_entry(_pick_entry(entries))

    build_includes: dict[str, None] = {}
    for entry in entries:
        if not _is_esphome_src(entry["file"]):
            continue
        for inc in _parse_entry(entry)[2]:
            build_includes.setdefault(inc, None)

    return {
        "cxx_path": cxx_path,
        "cxx_flags": cxx_flags,
        "defines": defines,
        "includes": {
            "build": list(build_includes),
            "toolchain": _get_toolchain_includes(cxx_path),
        },
    }
