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
from esphome.const import (
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_LN882X,
    PLATFORM_NRF52,
    PLATFORM_RTL87XX,
)
from esphome.helpers import write_file_if_changed

_LOGGER = logging.getLogger(__name__)

# The header and its sidecars live in the build directory
PCH_HEADER_NAME = "esphome_pch.h"
PCH_GCH_NAME = f"{PCH_HEADER_NAME}.gch"
# ccache hashes this instead of the .gch; also the freshness stamp
PCH_SUM_NAME = f"{PCH_GCH_NAME}.sum"
# The include list the .gch is compiled from
PCH_SOURCE_NAME = "esphome_pch_src.h"
_PCH_COMMAND_CACHE = f"{PCH_HEADER_NAME}.cmd.json"

# GCC can skip a .gch without a diagnostic and read the header of the same
# name, so that header is an error. Other tools get the include list.
PCH_GUARD_TEXT = f"""\
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTELLISENSE__)
#error "The precompiled header was not loaded"
#else
#include "{PCH_SOURCE_NAME}"
#endif
"""

# Every artifact the pch machinery can leave behind, for cleanup
PCH_ARTIFACT_NAMES = (
    PCH_HEADER_NAME,
    PCH_GCH_NAME,
    PCH_SUM_NAME,
    PCH_SOURCE_NAME,
    _PCH_COMMAND_CACHE,
)

# The core headers every backend precompiles
PCH_DEFAULT_HEADERS = ("esphome/core/pch_prefix.h",)

# PlatformIO platforms that do not take the pch script
PCH_SCRIPT_EXCLUDED_PLATFORMS = frozenset(
    {
        PLATFORM_BK72XX,
        PLATFORM_ESP32,
        PLATFORM_LN882X,
        PLATFORM_NRF52,
        PLATFORM_RTL87XX,
    }
)

# What ccache needs to cache compiles that load a .gch
_CCACHE_PCH_SLOPPINESS = ("pch_defines", "time_macros")

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
set(esphome_pch_sources {sources_var})
list(FILTER esphome_pch_sources EXCLUDE REGEX "[.][cSs]$")
set_source_files_properties(${{esphome_pch_sources}} PROPERTIES
    OBJECT_DEPENDS "${{CMAKE_BINARY_DIR}}/{PCH_HEADER_NAME}")
"""


def ccache_pch_env() -> dict[str, str]:
    """What ccache needs to cache compiles that load a .gch, added to what
    the user already set."""
    if not pch_enabled():
        return {}
    sloppiness = [
        item.strip()
        for item in os.environ.get("CCACHE_SLOPPINESS", "").split(",")
        if item.strip()
    ]
    sloppiness += [item for item in _CCACHE_PCH_SLOPPINESS if item not in sloppiness]
    env = {"CCACHE_SLOPPINESS": ",".join(sloppiness)}
    if "CCACHE_PCH_EXTSUM" not in os.environ:
        env["CCACHE_PCH_EXTSUM"] = "true"
    return env


def pch_script_enabled() -> bool:
    """Whether this PlatformIO build takes the pch script."""
    from esphome.core import CORE

    return pch_enabled() and CORE.target_platform not in PCH_SCRIPT_EXCLUDED_PLATFORMS


def pch_header_text(include_headers: Iterable[str]) -> str:
    """The prefix-header source: exactly these includes, in order."""
    return "".join(f'#include "{name}"\n' for name in include_headers)


def write_pch_headers(build_dir: Path, include_headers: Iterable[str]) -> Path:
    """Write the guard header and the include list; return the latter,
    which is what the .gch compiles from."""
    write_file_if_changed(build_dir / PCH_HEADER_NAME, PCH_GUARD_TEXT)
    source = build_dir / PCH_SOURCE_NAME
    write_file_if_changed(source, pch_header_text(include_headers))
    return source


def _include_closure(src_dir: Path, roots: Iterable[str]) -> dict[str, bytes]:
    """Include closure of ``roots``: src-relative name -> contents.

    Resolution mirrors the compiler (includer's dir, then src root). No
    #ifdef evaluation: including too much is the safe direction. Headers
    outside ``src_dir`` are covered by the version strings of the caller.
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
            if Path(inc).name != PCH_HEADER_NAME:
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
        .replace(str(CORE.build_path), "")
        .replace(effective_ccache_basedir(), "")
    )
    # The closure is sorted, so header order only enters via the text
    return pch_checksum(
        src_dir, include_headers, (pch_header_text(include_headers), *extra, flags)
    )


_DISABLE_HINT = " (set ESPHOME_PCH_ENABLE=0 to disable)"


def log_pch_in_use() -> None:
    _LOGGER.info("Compiling with a precompiled header%s", _DISABLE_HINT)


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
        source = write_pch_headers(build_dir, PCH_DEFAULT_HEADERS)
        cmd, cmd_dir = pch_compile_command(build_dir, source, gch)
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
                f"Could not compile the precompiled header{_DISABLE_HINT}: "
                f"{result.stderr.strip()}"
            )
        # Consumers depend on the header, so bump it to recompile them
        os.utime(header)
        sum_path.write_text(checksum + "\n", encoding="utf-8")
    except (OSError, ValueError, KeyError, IndexError) as err:
        raise EsphomeError(
            f"Could not prepare the precompiled header{_DISABLE_HINT}: {err}"
        ) from err
