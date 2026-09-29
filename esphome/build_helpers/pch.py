"""Shared precompiled header policy for the build backends."""

from __future__ import annotations

from collections.abc import Iterable, Sequence
import hashlib
import logging
import os
from pathlib import Path
import posixpath
import re
import subprocess
import sys

from esphome.build_helpers.ccache import effective_ccache_basedir, parse_enable_env
from esphome.const import PLATFORM_NRF52
from esphome.helpers import write_file_if_changed

_LOGGER = logging.getLogger(__name__)

# The header and its sidecars live in the build directory
PCH_HEADER_NAME = "esphome_pch.h"
PCH_GCH_NAME = f"{PCH_HEADER_NAME}.gch"
# ccache hashes this instead of the .gch; also the freshness stamp
PCH_SUM_NAME = f"{PCH_GCH_NAME}.sum"
# The include list the .gch is compiled from
PCH_SOURCE_NAME = "esphome_pch_src.h"

# GCC can skip a .gch without a diagnostic and read the header of the same
# name, so that header is an error. Other tools get the include list.
PCH_GUARD_TEXT = f"""\
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTELLISENSE__)
#error "The precompiled header was not loaded"
#else
#include "{PCH_SOURCE_NAME}"
#endif
"""

# The cc1plus wrapper the PlatformIO script writes on arm64 macOS
PCH_CC1_DIR = "pch_cc1"

# What the PlatformIO script leaves in the project root, for cleanup
PCH_ARTIFACT_NAMES = (PCH_HEADER_NAME, PCH_GCH_NAME, PCH_SUM_NAME, PCH_SOURCE_NAME)
PCH_ARTIFACT_DIRS = (PCH_CC1_DIR,)

# The core headers every backend precompiles
PCH_DEFAULT_HEADERS = ("esphome/core/pch_prefix.h",)

# PlatformIO platforms that do not take the pch script
PCH_SCRIPT_EXCLUDED_PLATFORMS = frozenset(
    {
        PLATFORM_NRF52,
    }
)

# What ccache needs to cache compiles that load a .gch
_CCACHE_PCH_SLOPPINESS = ("pch_defines", "time_macros")

# Both include forms: an angle include resolving under src/ enters the digest
_INCLUDE_RE = re.compile(rb'^\s*#\s*include\s+["<]([^">]+)[">]', re.MULTILINE)


def pch_enabled() -> bool:
    """Precompiled-header knob: default on, ``ESPHOME_PCH_ENABLE=0`` opts out."""
    return parse_enable_env("ESPHOME_PCH_ENABLE") is not False


def pch_forced() -> bool:
    """``ESPHOME_PCH_ENABLE=1``: wanted even where the host rule says no."""
    return parse_enable_env("ESPHOME_PCH_ENABLE") is True


# GCC bug 14940: before these releases the Windows loader maps a .gch only
# at its saved address. First fixed release per major, 16 on always fixed;
# PCH_WINDOWS_CMAKE_OLD_GCC and the pch_usable message spell the same table
PCH_WINDOWS_GCC_FIXED = {14: (14, 4), 15: (15, 3)}
PCH_WINDOWS_GCC_FIXED_DEFAULT = (16, 0)
# The same rule for CMake, which alone knows the version before configure
PCH_WINDOWS_CMAKE_OLD_GCC = (
    "CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14.4 OR "
    "(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15 AND "
    "CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.3)"
)


def gcc_relocates_pch_on_windows(version: Sequence[int]) -> bool:
    """Whether a GCC of this version loads a .gch on Windows."""
    if not version:
        return False
    fixed = PCH_WINDOWS_GCC_FIXED.get(version[0], PCH_WINDOWS_GCC_FIXED_DEFAULT)
    return tuple(version[:2]) >= fixed


def gcc_version(cxx: Sequence[Path | str]) -> tuple[int, ...]:
    """What ``-dumpfullversion`` says, or () when the compiler cannot run."""
    try:
        result = subprocess.run(
            [*cxx, "-dumpfullversion"], capture_output=True, text=True, check=False
        )
    except OSError as err:
        _LOGGER.debug("Cannot run %s: %s", cxx[0], err)
        return ()
    parts = result.stdout.strip().split(".")
    if not all(part.isdigit() for part in parts):
        return ()
    return tuple(int(part) for part in parts)


def pch_needs_gcc_check() -> bool:
    """Windows host with the knob unset: the compiler version decides."""
    return sys.platform == "win32" and parse_enable_env("ESPHOME_PCH_ENABLE") is None


def pch_usable(cxx: Sequence[Path | str]) -> bool:
    """The knob plus the host rule; ``ESPHOME_PCH_ENABLE=1`` skips the rule."""
    if not pch_enabled():
        return False
    if not pch_needs_gcc_check():
        return True
    version = gcc_version(cxx)
    if gcc_relocates_pch_on_windows(version):
        return True
    _LOGGER.info(
        "GCC %s cannot load a precompiled header on Windows (GCC bug 14940, "
        "fixed in 14.4, 15.3 and 16); compiling without it "
        "(set ESPHOME_PCH_ENABLE=1 to force)",
        ".".join(map(str, version)) or "of unknown version",
    )
    return False


def pch_consumer_flags() -> list[str]:
    """Flags a C++ src compile loads the pch with. The -include stays
    relative: an absolute path would enter the ccache key."""
    return ["-Winvalid-pch", "-Werror=invalid-pch", "-include", PCH_HEADER_NAME]


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
