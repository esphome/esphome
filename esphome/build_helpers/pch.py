"""Shared precompiled header policy for the build backends."""

from __future__ import annotations

from collections.abc import Iterable
import hashlib
import logging
import os
from pathlib import Path
import posixpath
import re

from esphome.build_helpers.ccache import parse_enable_env

_LOGGER = logging.getLogger(__name__)

# The core headers every backend precompiles
PCH_DEFAULT_HEADERS = ("esphome/core/pch_prefix.h",)

# What ccache needs to cache compiles that load a .gch
_CCACHE_PCH_SLOPPINESS = ("pch_defines", "time_macros")

# Both include forms: an angle include resolving under src/ enters the digest
_INCLUDE_RE = re.compile(rb'^\s*#\s*include\s+["<]([^">]+)[">]', re.MULTILINE)


def pch_enabled() -> bool:
    """Precompiled-header knob: default on, ``ESPHOME_PCH_ENABLE=0`` opts out."""
    return parse_enable_env("ESPHOME_PCH_ENABLE") is not False


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


_DISABLE_HINT = " (set ESPHOME_PCH_ENABLE=0 to disable)"


def log_pch_in_use() -> None:
    _LOGGER.info("Compiling with a precompiled header%s", _DISABLE_HINT)
