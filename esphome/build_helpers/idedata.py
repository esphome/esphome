"""Derive idedata from a native (non-PlatformIO) build's ``compile_commands.json``.

PlatformIO exposes a curated ``pio run -t idedata`` JSON; the native
toolchains have no such command, but each build produces a
``compile_commands.json`` (CMAKE_EXPORT_COMPILE_COMMANDS for ESP-IDF, ninja's
compdb tool otherwise). This module turns that file into the same fields
consumers (IDE integration, clang-tidy) expect:

    {cc_path, cxx_path, cxx_flags, defines, includes: {build, toolchain}}
"""

from __future__ import annotations

import json
import logging
import os
from pathlib import Path
import shlex
import subprocess

from esphome.core import EsphomeError
from esphome.helpers import write_file

# Everything idedata generation may raise after a successful link; idedata
# is a bonus artifact, so consumers warn instead of failing the build
IDEDATA_BEST_EFFORT_ERRORS = (
    EsphomeError,
    LookupError,
    OSError,
    RuntimeError,
    ValueError,
)

_LOGGER = logging.getLogger(__name__)

# C++ translation-unit suffixes used to identify ESPHome source files.
_CXX_SUFFIXES = (".cpp", ".cc")
# Suffixes of input/output files that appear bare on the command line (and so
# must not be mistaken for compiler flags).
_INPUT_FILE_SUFFIXES = (*_CXX_SUFFIXES, ".c", ".o", ".S", ".s")
# Path marker identifying an ESPHome source translation unit.
_ESPHOME_SRC_MARKER = "/src/esphome/"


def _is_esphome_src(file: str) -> bool:
    """Whether ``file`` is an ESPHome C++ translation unit; normalized to
    ``/`` first since Windows compile DBs use backslashes."""
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
    """Pick a representative ESPHome C++ TU; all share the same component
    flags/defines."""
    for entry in entries:
        if _is_esphome_src(entry["file"]):
            return entry
    for entry in entries:
        if entry["file"].endswith(_CXX_SUFFIXES):
            return entry
    raise ValueError("no C++ translation unit found in compile_commands.json")


# Compiler launchers that may prefix a compile command; a closed launcher
# denylist beats enumerating compiler names, an open set.
_LAUNCHER_STEMS = frozenset({"ccache", "sccache", "distcc", "icecc", "buildcache"})


def _is_launcher(token: str) -> bool:
    return Path(token).stem.lower() in _LAUNCHER_STEMS


def parse_entry(
    entry: dict, launcher: str | None = None
) -> tuple[str, list[str], list[str], list[str]]:
    """Parse one compile_commands entry -> (cxx_path, defines, includes, cxx_flags)."""
    directory = Path(entry["directory"])
    tokens = _expand_response_files(_split_command(entry["command"]), directory)

    def _include(raw: str) -> str:
        # Resolve against the entry's ``directory`` so cached idedata works
        # from any cwd; emit forward slashes to match the JSON's own entries
        raw = raw.strip()
        if raw and not Path(raw).is_absolute():
            raw = os.path.normpath(directory / raw)
        return raw.replace("\\", "/")

    # A launcher-wrapped command ("ccache g++ ...") names the compiler second
    if launcher is not None and tokens[:1] == [launcher]:
        tokens = tokens[1:]
    if not tokens:
        # An empty command, or one that was only the launcher; fail by name
        raise ValueError(f"empty compile command for {entry.get('file')}")
    if _is_launcher(tokens[0]) and len(tokens) > 1 and not tokens[1].startswith("-"):
        # Stale DB built with a launcher this run no longer configures; the
        # real compiler is the next token
        _LOGGER.warning("Stripping unconfigured launcher %s", tokens[0])
        tokens = tokens[1:]
    # token0 is the compiler path; the rest of the command already uses forward
    # slashes on Windows, so normalize it too for a consistent idedata file.
    cxx_path = tokens[0].replace("\\", "/")
    # Enforced here so no caller can record ccache as the compiler
    reject_launcher_compiler(cxx_path)
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


def get_toolchain_includes(cxx_path: str) -> list[str]:
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


def _cc_path_from_cxx(cxx_path: str) -> str:
    """Derive the C compiler path from the C++ compiler path.

    compile_commands.json only names the C++ compiler, but consumers reach the
    rest of the toolchain (objdump, readelf, addr2line) by rewriting the tail of
    ``cc_path``, so they need the ``gcc``-suffixed name.
    """
    stem, suffix = (
        (cxx_path[: -len(".exe")], ".exe")
        if cxx_path.endswith(".exe")
        else (cxx_path, "")
    )
    # Rewrite the program name only when it is g++ itself, or a toolchain
    # prefixed one such as xtensa-esp32-elf-g++ -> xtensa-esp32-elf-gcc.
    # Requiring a separator before the "g++" keeps names that merely end in
    # those three characters intact: "clang++" must not become "clangcc".
    head = stem[: -len("g++")]
    if stem.endswith("g++") and (not head or head.endswith(("-", "/", "\\"))):
        stem = f"{head}gcc"
    return f"{stem}{suffix}"


def _cache_usable(cached: object) -> bool:
    """Check a cached idedata dict against the guarantees of the write path.

    Caches written by older versions predate the launcher rejection and the
    include-union shape; serving one would bypass both. The dict check also
    keeps "in" from substring-matching a bare JSON string.
    """
    if not isinstance(cached, dict) or "cc_path" not in cached:
        return False
    cxx_path = cached.get("cxx_path")
    if not isinstance(cxx_path, str) or _is_launcher(cxx_path):
        return False
    includes = cached.get("includes")
    return isinstance(includes, dict) and isinstance(includes.get("build"), list)


def load_or_build_idedata(
    compile_commands: Path,
    elf_path: Path,
    cache: Path,
    launcher: str | None = None,
) -> dict | None:
    """Return idedata for a compile_commands.json build, cached on mtime.

    Shared by the native ESP-IDF and ESP8266 Arduino toolchains. Returns None
    when the compile DB doesn't exist yet (nothing was built). ``launcher``
    is the compiler-launcher path (ccache) the build was generated with, if
    any; commands in the compile DB are prefixed with it.
    """
    if not compile_commands.is_file():
        _LOGGER.debug("No %s yet; skipping idedata generation", compile_commands)
        return None

    if cache.is_file() and cache.stat().st_mtime >= compile_commands.stat().st_mtime:
        try:
            cached = json.loads(cache.read_text(encoding="utf-8"))
        except (ValueError, OSError) as err:
            # A recurring cause (interrupted write, disk full) would otherwise
            # look like unexplained slow builds
            _LOGGER.warning("Discarding unreadable idedata cache %s: %s", cache, err)
        else:
            if _cache_usable(cached):
                # Re-stamp so a relocated build dir cannot serve a stale ELF path
                cached["prog_path"] = str(elf_path)
                return cached
            _LOGGER.debug("Regenerating idedata: cache %s fails validation", cache)

    data = idedata_from_build(compile_commands, launcher)
    data["prog_path"] = str(elf_path)
    cache.parent.mkdir(parents=True, exist_ok=True)
    # Atomic so a crash mid-write cannot leave a truncated cache
    write_file(cache, json.dumps(data, indent=2) + "\n")
    return data


def reject_launcher_compiler(cxx_path: str) -> None:
    """Reject a compile DB naming a launcher (ccache) as the compiler; it
    must never be probed, cached, or consumed."""
    if _is_launcher(cxx_path):
        raise EsphomeError(
            f"compile_commands.json names the launcher {cxx_path} as the "
            "compiler; the compile database is unusable"
        )


def idedata_from_build(compile_commands: Path, launcher: str | None = None) -> dict:
    """Parse compile_commands.json into the idedata fields consumers expect.

    A single compile entry only carries the include set its own translation
    unit was built with (per-component under ESP-IDF), but consumers
    (clang-tidy) analyze ESPHome headers that transitively pull in other
    components. So take cxx_path / cxx_flags / defines from a representative
    ESPHome TU, but union the include dirs across all ESPHome TUs to get a
    project-wide superset (as PlatformIO's idedata provides).
    """
    entries = json.loads(Path(compile_commands).read_text(encoding="utf-8"))
    if not isinstance(entries, list) or not all(isinstance(e, dict) for e in entries):
        # A TypeError here would escape IDEDATA_BEST_EFFORT_ERRORS
        raise EsphomeError(f"{compile_commands} is not a compile-command list")

    representative = _pick_entry(entries)
    cxx_path, defines, rep_includes, cxx_flags = parse_entry(representative, launcher)

    # Seed with the representative's includes so it is not parsed twice
    has_esphome_tu = _is_esphome_src(representative["file"])
    build_includes: dict[str, None] = dict.fromkeys(
        rep_includes if has_esphome_tu else ()
    )

    def _shape(entry: dict) -> str:
        # directory + command minus TU-specific paths: same shape means the
        # same include set, so tokenize once per shape. Response-file
        # commands never dedupe (the .rsp contents differ per object)
        command = entry["command"]
        directory = entry.get("directory", "")
        if "@" in command:
            return f"unique:{directory}|{entry.get('output') or command}"
        stripped = command.replace(entry.get("file", ""), "").replace(
            entry.get("output", ""), ""
        )
        return f"{directory}|{stripped}"

    seen_shapes = {_shape(representative)}
    for entry in entries:
        if entry is representative or not _is_esphome_src(entry["file"]):
            continue
        has_esphome_tu = True
        if (shape := _shape(entry)) in seen_shapes:
            _LOGGER.debug("Include union: %s shares a command shape", entry["file"])
            continue
        seen_shapes.add(shape)
        for inc in parse_entry(entry, launcher)[2]:
            build_includes.setdefault(inc, None)

    if not has_esphome_tu:
        # An arbitrary fallback TU breaks clang-tidy/IDE consumers, and a
        # warning would be cached into permanence; call sites downgrade this
        raise EsphomeError(
            f"No ESPHome translation unit found in {compile_commands}; "
            "refusing to cache unusable idedata"
        )

    return {
        "cc_path": _cc_path_from_cxx(cxx_path),
        "cxx_path": cxx_path,
        "cxx_flags": cxx_flags,
        "defines": defines,
        "includes": {
            "build": list(build_includes),
            "toolchain": get_toolchain_includes(cxx_path),
        },
    }
