"""Shared precompiled-header policy for the build backends.

The prefix either mirrors the TUs' own force-includes (ESP8266) or is a
curated core-header set (ESP-IDF). ``esphome: includes:`` sources receive
it too; Arduino.h visibility there is intended (esphome#8693).
"""

from __future__ import annotations

from collections.abc import Iterable
from contextlib import suppress
import hashlib
import json
import logging
import os
from pathlib import Path
import posixpath
import re
import stat
import subprocess

from esphome.build_helpers.ccache import effective_ccache_basedir, parse_enable_env
from esphome.build_helpers.idedata import (
    CXX_SOURCE_SUFFIXES,
    expand_response_files,
    is_launcher,
    split_command,
)
from esphome.helpers import write_file_if_changed

_LOGGER = logging.getLogger(__name__)

# The header and its sidecars live in the build directory
PCH_HEADER_NAME = "esphome_pch.h"
_PCH_COMMAND_CACHE = f"{PCH_HEADER_NAME}.cmd.json"

# The core defines header every backend anchors its prefix on
PCH_CORE_HEADER = "esphome/core/defines.h"

# The curated core headers, for backends without framework force-includes
PCH_PREFIX_HEADER = "esphome/core/pch_prefix.h"

# defines.h first so USE_* macros exist for the rest. The curated headers
# become visible in every src TU; ESPHOME_PCH_ENABLE=0 restores the strict view
PCH_DEFAULT_HEADERS = (PCH_CORE_HEADER, PCH_PREFIX_HEADER)

# ccache cannot hash through a .gch; CCACHE_PCH_EXTSUM makes it hash the
# .sum sidecar instead of the .gch bytes, which are not reproducible
CCACHE_PCH_ENV = {
    "CCACHE_SLOPPINESS": "pch_defines,time_macros",
    "CCACHE_PCH_EXTSUM": "true",
}

# Both include forms: an angle include resolving under src/ enters the digest
_INCLUDE_RE = re.compile(rb'^\s*#\s*include\s+["<]([^">]+)[">]', re.MULTILINE)


def pch_enabled() -> bool:
    """Precompiled-header knob: default on, ``ESPHOME_PCH_ENABLE=0`` opts out."""
    return parse_enable_env("ESPHOME_PCH_ENABLE") is not False


def pch_consumer_flags() -> list[str]:
    """Flags a C++ src compile loads the pch with.

    The -include stays relative: an absolute path would poison ccache keys.
    A .gch the compiler rejects is an error, never a silent text include.
    """
    return ["-Winvalid-pch", "-Werror=invalid-pch", "-include", PCH_HEADER_NAME]


def pch_cmake_consumer(target: str, sources_var: str) -> str:
    """The CMake block making ``target``'s C++ sources consume the pch;
    empty when disabled. OBJECT_DEPENDS is on the header, not the .gch
    (pch-baked headers drop out of TU depfiles)."""
    if not pch_enabled():
        return ""
    options = "\n".join(
        f'    "$<$<COMPILE_LANGUAGE:CXX>:{flag}>"' for flag in pch_consumer_flags()
    )
    return f"""
# ESPHome precompiled header (see esphome/build_helpers/pch.py).
# The touch keeps OBJECT_DEPENDS satisfiable when the build system itself
# wiped the build dir after the header was written (west --pristine)
if(NOT EXISTS "${{CMAKE_BINARY_DIR}}/{PCH_HEADER_NAME}")
  file(TOUCH "${{CMAKE_BINARY_DIR}}/{PCH_HEADER_NAME}")
endif()
target_compile_options({target} PRIVATE
{options}
)
set_source_files_properties({sources_var} PROPERTIES
    OBJECT_DEPENDS "${{CMAKE_BINARY_DIR}}/{PCH_HEADER_NAME}")
"""


def ccache_pch_env() -> dict[str, str]:
    """Settings ccache needs to cache compiles that consume the .gch.

    User-set values win. Only time_macros affects TUs without the pch.
    """
    if not pch_enabled():
        return {}
    return {k: v for k, v in CCACHE_PCH_ENV.items() if k not in os.environ}


def pch_header_text(include_headers: Iterable[str]) -> str:
    """The prefix-header source: exactly these includes, in order."""
    return "".join(f'#include "{name}"\n' for name in include_headers)


def _resolves(path: Path) -> bool:
    """False when missing; other stat failures propagate (identity unknown,
    unlike is_file(), which would silently drop the header)."""
    try:
        return stat.S_ISREG(path.stat().st_mode)
    except (FileNotFoundError, NotADirectoryError):
        return False


def _include_closure(src_dir: Path, roots: Iterable[str]) -> dict[str, bytes]:
    """Include closure of ``roots``: src-relative name -> contents.

    Resolution mirrors the compiler (includer's dir, then src root); names
    outside ``src_dir`` end the walk and are versioned by the caller. No
    #ifdef evaluation: over-approximating is the safe direction.
    """
    seen: dict[str, bytes] = {}
    stack: list[tuple[str, str]] = [(name, "") for name in roots]
    while stack:
        name, from_dir = stack.pop()
        for candidate in (f"{from_dir}/{name}" if from_dir else name, name):
            rel = posixpath.normpath(candidate)
            if not rel.startswith("..") and _resolves(src_dir / rel):
                break
        else:
            continue
        if rel in seen:
            continue
        try:
            data = (src_dir / rel).read_bytes()
        except OSError as err:
            # A marker would truncate the transitive walk; fail closed
            _LOGGER.warning("Could not read %s for the pch checksum: %s", rel, err)
            raise
        seen[rel] = data
        parent = posixpath.dirname(rel)
        stack.extend(
            # surrogateescape: a non-UTF-8 name just fails to resolve
            (inc.decode(errors="surrogateescape"), parent)
            for inc in _INCLUDE_RE.findall(data)
        )
    return seen


def pch_checksum(
    src_dir: Path, include_headers: Iterable[str], extra: Iterable[str]
) -> str:
    """Digest standing in for the .gch in ccache's hash: the include closure
    of the prefix header plus caller-supplied identity strings (versioned
    install paths, flags). Raises OSError when a header's identity cannot
    be established at all; callers must then compile without a pch."""
    digest = hashlib.sha256()
    closure = _include_closure(src_dir, include_headers)
    for name in sorted(closure):
        digest.update(name.encode(errors="surrogateescape"))
        digest.update(closure[name])
        digest.update(b"\0")
    for item in extra:
        digest.update(item.encode(errors="surrogateescape"))
        digest.update(b"\0")
    return digest.hexdigest()


# Tokens dropped when retargeting a TU's flags at the prefix header
# (the pch compile must not touch depfiles)
_PCH_STRIP_FLAGS_WITH_ARG = frozenset({"-o", "-c", "-MT", "-MF", "-MQ"})
_PCH_STRIP_FLAGS = frozenset({"-MD", "-MMD", "-MP", "-MM", "-M"})


def _src_compile_entry(build_dir: Path) -> tuple[str, str]:
    """The command and directory of one src C++ compile.

    The compile database is tens of MB and only changes on a reconfigure,
    so the one entry used is cached next to it.
    """
    from esphome.core import CORE, EsphomeError

    database = build_dir / "compile_commands.json"
    cache = build_dir / _PCH_COMMAND_CACHE
    stamp = database.stat()
    key = [stamp.st_mtime_ns, stamp.st_size]
    with suppress(OSError, ValueError, KeyError):
        cached = json.loads(cache.read_text(encoding="utf-8"))
        if cached["key"] == key:
            return cached["command"], cached["directory"]
    entries = json.loads(database.read_text(encoding="utf-8"))
    # CMake may spell paths through a symlink differently than CORE does
    # (macOS /tmp vs /private/tmp), so compare resolved paths
    src_root = Path(CORE.relative_src_path()).resolve()
    entry = next(
        (
            e
            for e in entries
            if e["file"].endswith(CXX_SOURCE_SUFFIXES)
            and Path(e["file"]).resolve().is_relative_to(src_root)
        ),
        None,
    )
    if entry is None:
        raise EsphomeError(f"{database} has no C++ source from {src_root}")
    found = entry["command"], entry["directory"]
    cache.write_text(
        json.dumps({"key": key, "command": found[0], "directory": found[1]}),
        encoding="utf-8",
    )
    return found


def pch_compile_command(
    build_dir: Path, header: Path, gch: Path
) -> tuple[list[str], Path]:
    """The exact src C++ flags from compile_commands.json retargeted at the
    header, with the directory they resolve against (relative -I paths must
    be expanded and executed from the same root)."""
    command, directory = _src_compile_entry(build_dir)
    cmd_dir = Path(directory)
    tokens = expand_response_files(split_command(command), cmd_dir)
    # A DB recorded with ccache enabled prefixes the compiler with the
    # launcher; the .gch must be compiled directly
    if is_launcher(tokens[0]):
        tokens = tokens[1:]
    args: list[str] = []
    arg_it = iter(tokens)
    for tok in arg_it:
        if tok in _PCH_STRIP_FLAGS_WITH_ARG:
            next(arg_it, None)
            continue
        if tok in _PCH_STRIP_FLAGS:
            continue
        if tok == "-include":
            # Drop only the injected prefix; user force-includes must reach
            # the .gch compile or GCC rejects it over the macro mismatch
            inc = next(arg_it, "")
            if not inc.endswith(PCH_HEADER_NAME):
                args.extend(("-include", inc))
            continue
        args.append(tok)
    return [*args, "-x", "c++-header", "-c", str(header), "-o", str(gch)], cmd_dir


def _flags_identity(tokens: Iterable[str]) -> str:
    """Flag string normalized for digest use: strip like ccache's rewriting
    (user CCACHE_BASEDIR wins); the raw build path covers unresolved
    (symlinked) spellings."""
    from esphome.core import CORE

    return (
        " ".join(tokens)
        .replace(effective_ccache_basedir(), "")
        .replace(str(CORE.build_path), "")
    )


def pch_identity(
    tokens: Iterable[str],
    src_dir: Path,
    include_headers: tuple[str, ...],
    extra: Iterable[str],
) -> str:
    """The .sum digest naming this exact pch build: include closure, header
    text, backend identity strings, and the normalized compile command or
    flags (``tokens``)."""
    return pch_checksum(
        src_dir,
        include_headers,
        (
            # The closure is sorted, so root order only enters via the text
            pch_header_text(include_headers),
            *extra,
            _flags_identity(tokens),
        ),
    )


def log_pch_in_use() -> None:
    # The only place a user can discover the knob
    _LOGGER.info(
        "Compiling with a precompiled header (set ESPHOME_PCH_ENABLE=0 to disable)"
    )


def prepare_pch(build_dir: Path, identity_file: Path, extra: Iterable[str]) -> None:
    """Compile ``build_dir``'s .gch from compile_commands.json flags and
    write its ccache .sum.

    The .sum doubles as the freshness stamp and folds in the compile
    command, so a flag-only change rebuilds the .gch. ``identity_file`` is
    the backend's settled configuration (sdkconfig, autoconf.h) and
    ``extra`` the rest of its identity (framework version, flags). The pch
    holds only ESPHome's own headers, so a failure is a defect: it raises.
    Callers check ``pch_enabled()``.
    """
    from esphome.core import CORE, EsphomeError

    build_dir.mkdir(parents=True, exist_ok=True)
    header = build_dir / PCH_HEADER_NAME
    write_file_if_changed(header, pch_header_text(PCH_DEFAULT_HEADERS))
    gch = Path(f"{header}.gch")
    sum_path = Path(f"{gch}.sum")
    cmd, cmd_dir = pch_compile_command(build_dir, header, gch)
    checksum = pch_identity(
        cmd,
        CORE.relative_src_path(),
        PCH_DEFAULT_HEADERS,
        (*extra, identity_file.read_text(encoding="utf-8")),
    )
    log_pch_in_use()
    with suppress(OSError, UnicodeDecodeError):
        if gch.is_file() and sum_path.read_text(encoding="utf-8").strip() == checksum:
            return
    result = subprocess.run(
        cmd, cwd=cmd_dir, capture_output=True, text=True, check=False
    )
    if result.returncode != 0 or not gch.is_file():
        error = result.stderr.strip() or f"exit code {result.returncode}"
        raise EsphomeError(f"Could not compile the precompiled header: {error}")
    sum_path.write_text(checksum + "\n", encoding="utf-8")
    # Consumers depend on the header (depfiles cannot see through a .gch);
    # bump it so users of the previous .gch recompile
    os.utime(header)
