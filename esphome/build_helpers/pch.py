"""Shared precompiled header policy for the build backends."""

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
import subprocess

from esphome.build_helpers.ccache import effective_ccache_basedir, parse_enable_env
from esphome.build_helpers.idedata import (
    expand_response_files,
    is_esphome_src,
    is_launcher,
    split_command,
)
from esphome.helpers import write_file_if_changed

_LOGGER = logging.getLogger(__name__)

# The header and its sidecars live in the build directory
PCH_HEADER_NAME = "esphome_pch.h"
PCH_GCH_NAME = f"{PCH_HEADER_NAME}.gch"
# ccache hashes this instead of the .gch; also the freshness stamp
PCH_SUM_NAME = f"{PCH_GCH_NAME}.sum"
_PCH_COMMAND_CACHE = f"{PCH_HEADER_NAME}.cmd.json"

PCH_CORE_HEADER = "esphome/core/defines.h"
# The curated core headers
PCH_PREFIX_HEADER = "esphome/core/pch_prefix.h"
# defines.h first so USE_* macros exist for the rest
PCH_DEFAULT_HEADERS = (PCH_CORE_HEADER, PCH_PREFIX_HEADER)

# What ccache needs to cache compiles that load a .gch
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
    """Flags a C++ src compile loads the pch with. The -include stays
    relative: an absolute path would enter the ccache key."""
    return ["-Winvalid-pch", "-Werror=invalid-pch", "-include", PCH_HEADER_NAME]


def pch_cmake_consumer(target: str, sources_var: str) -> str:
    """The CMake block making ``target``'s C++ sources load the pch; empty
    when disabled."""
    if not pch_enabled():
        return ""
    options = "\n".join(
        f'    "$<$<COMPILE_LANGUAGE:CXX>:{flag}>"' for flag in pch_consumer_flags()
    )
    return f"""
# Depfiles cannot see through a .gch, so depend on its header
target_compile_options({target} PRIVATE
{options}
)
set_source_files_properties({sources_var} PROPERTIES
    OBJECT_DEPENDS "${{CMAKE_BINARY_DIR}}/{PCH_HEADER_NAME}")
"""


def ccache_pch_env() -> dict[str, str]:
    """``CCACHE_PCH_ENV`` without what the user already set."""
    if not pch_enabled():
        return {}
    return {k: v for k, v in CCACHE_PCH_ENV.items() if k not in os.environ}


def pch_header_text(include_headers: Iterable[str]) -> str:
    """The prefix-header source: exactly these includes, in order."""
    return "".join(f'#include "{name}"\n' for name in include_headers)


def _include_closure(src_dir: Path, roots: Iterable[str]) -> dict[str, bytes]:
    """Include closure of ``roots``: src-relative name -> contents.

    Resolution mirrors the compiler (includer's dir, then src root). No
    #ifdef evaluation: including too much is the safe direction.
    """
    seen: dict[str, bytes] = {}
    stack: list[tuple[str, str]] = [(name, "") for name in roots]
    while stack:
        name, from_dir = stack.pop()
        for candidate in (f"{from_dir}/{name}" if from_dir else name, name):
            rel = posixpath.normpath(candidate)
            if not rel.startswith("..") and (src_dir / rel).is_file():
                break
        else:
            continue
        if rel in seen:
            continue
        data = seen[rel] = (src_dir / rel).read_bytes()
        parent = posixpath.dirname(rel)
        stack.extend((inc.decode(), parent) for inc in _INCLUDE_RE.findall(data))
    return seen


def pch_checksum(
    src_dir: Path, include_headers: Iterable[str], extra: Iterable[str]
) -> str:
    """Digest of the prefix header's include closure plus ``extra``."""
    digest = hashlib.sha256()
    closure = _include_closure(src_dir, include_headers)
    for name in sorted(closure):
        digest.update(name.encode())
        digest.update(closure[name])
        digest.update(b"\0")
    for item in extra:
        digest.update(item.encode())
        digest.update(b"\0")
    return digest.hexdigest()


# Dropped when retargeting a TU's flags at the prefix header
_PCH_STRIP_FLAGS_WITH_ARG = frozenset({"-o", "-c", "-MT", "-MF", "-MQ"})
_PCH_STRIP_FLAGS = frozenset({"-MD", "-MMD", "-MP", "-MM", "-M"})


def _src_compile_entry(build_dir: Path) -> tuple[str, str]:
    """The command and directory of one src C++ compile, cached because
    the compile database is tens of MB."""
    from esphome.core import EsphomeError

    database = build_dir / "compile_commands.json"
    cache = build_dir / _PCH_COMMAND_CACHE
    stamp = database.stat()
    key = [stamp.st_mtime_ns, stamp.st_size]
    with suppress(OSError, ValueError, KeyError, TypeError):
        cached = json.loads(cache.read_text(encoding="utf-8"))
        if cached["key"] == key:
            return cached["command"], cached["directory"]
    entries = json.loads(database.read_text(encoding="utf-8"))
    entry = next((e for e in entries if is_esphome_src(e["file"])), None)
    if entry is None:
        raise EsphomeError(f"{database} has no ESPHome C++ source")
    found = entry["command"], entry["directory"]
    cache.write_text(
        json.dumps({"key": key, "command": found[0], "directory": found[1]}),
        encoding="utf-8",
    )
    return found


def pch_compile_command(
    build_dir: Path, header: Path, gch: Path
) -> tuple[list[str], Path]:
    """A src C++ compile's flags retargeted at the header, and the
    directory they resolve against."""
    command, directory = _src_compile_entry(build_dir)
    cmd_dir = Path(directory)
    tokens = expand_response_files(split_command(command), cmd_dir)
    # The .gch is compiled without the ccache launcher
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
            # Drop only the pch itself; user force-includes must stay
            inc = next(arg_it, "")
            if not inc.endswith(PCH_HEADER_NAME):
                args.extend(("-include", inc))
            continue
        args.append(tok)
    return [*args, "-x", "c++-header", "-c", str(header), "-o", str(gch)], cmd_dir


def pch_identity(
    tokens: Iterable[str],
    src_dir: Path,
    include_headers: tuple[str, ...],
    extra: Iterable[str],
) -> str:
    """The .sum digest: include closure, header text, ``extra`` and the
    compile flags with the build path stripped, as ccache does."""
    from esphome.core import CORE

    flags = (
        " ".join(tokens)
        .replace(effective_ccache_basedir(), "")
        .replace(str(CORE.build_path), "")
    )
    # The closure is sorted, so header order only enters via the text
    return pch_checksum(
        src_dir, include_headers, (pch_header_text(include_headers), *extra, flags)
    )


def log_pch_in_use() -> None:
    # The only place a user can discover the knob
    _LOGGER.info(
        "Compiling with a precompiled header (set ESPHOME_PCH_ENABLE=0 to disable)"
    )


def prepare_pch(build_dir: Path, identity_file: Path, extra: Iterable[str]) -> None:
    """Compile ``build_dir``'s .gch from its compile_commands.json flags.

    ``identity_file`` is the backend's settled configuration (sdkconfig,
    autoconf.h) and ``extra`` the rest of its identity. Any failure raises.
    Callers check ``pch_enabled()``.
    """
    from esphome.core import CORE, EsphomeError

    header = build_dir / PCH_HEADER_NAME
    gch = build_dir / PCH_GCH_NAME
    sum_path = build_dir / PCH_SUM_NAME
    try:
        write_file_if_changed(header, pch_header_text(PCH_DEFAULT_HEADERS))
        cmd, cmd_dir = pch_compile_command(build_dir, header, gch)
        checksum = pch_identity(
            cmd,
            CORE.relative_src_path(),
            PCH_DEFAULT_HEADERS,
            (*extra, identity_file.read_text(encoding="utf-8")),
        )
        log_pch_in_use()
        if (
            gch.is_file()
            and sum_path.is_file()
            and sum_path.read_text(encoding="utf-8").strip() == checksum
        ):
            return
        result = subprocess.run(
            cmd, cwd=cmd_dir, capture_output=True, text=True, check=False
        )
        if result.returncode != 0:
            raise EsphomeError(
                f"Could not compile the precompiled header: {result.stderr.strip()}"
            )
        sum_path.write_text(checksum + "\n", encoding="utf-8")
        # Consumers depend on the header, so bump it to recompile them
        os.utime(header)
    except OSError as err:
        raise EsphomeError(f"Could not prepare the precompiled header: {err}") from err
