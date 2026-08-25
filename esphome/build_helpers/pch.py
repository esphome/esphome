"""Shared precompiled-header policy for the build backends.

Safe by construction when the prefix header mirrors what the TUs already
include first (ESP8266); a backend may instead inject a curated set of
self-contained core headers (ESP-IDF).
"""

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

# The header and its .gch/.sum sidecars live in the build directory.
PCH_HEADER_NAME = "esphome_pch.h"

# The core defines header every backend anchors its prefix on.
PCH_CORE_HEADER = "esphome/core/defines.h"

# ccache cannot hash through a .gch; CCACHE_PCH_EXTSUM makes it hash the
# .sum sidecar instead of the .gch bytes, which are not reproducible.
# Keep in sync with the literals in platformio/pch.py.script.
_CCACHE_PCH_ENV = {
    "CCACHE_SLOPPINESS": "pch_defines,time_macros",
    "CCACHE_PCH_EXTSUM": "true",
}

_INCLUDE_RE = re.compile(rb'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)


def pch_enabled() -> bool:
    """Precompiled-header knob: default on, ``ESPHOME_PCH_ENABLE=0`` opts out."""
    return parse_enable_env("ESPHOME_PCH_ENABLE") is not False


def ccache_pch_env() -> dict[str, str]:
    """Settings ccache needs to cache compiles that consume the .gch;
    empty when the pch is disabled. User-set values win. Native backends
    export these process-wide; only time_macros affects non-pch TUs."""
    if not pch_enabled():
        return {}
    return {k: v for k, v in _CCACHE_PCH_ENV.items() if k not in os.environ}


def pch_header_text(include_headers: Iterable[str]) -> str:
    """The prefix-header source: exactly these includes, in order."""
    return "".join(f'#include "{name}"\n' for name in include_headers)


def _include_closure(src_dir: Path, roots: Iterable[str]) -> dict[str, bytes]:
    """Quoted-include closure of ``roots``: src-relative name -> contents.

    Resolves each include against the includer's directory first, then the
    src root, matching the compiler's quoted-include lookup. Names that do
    not resolve under ``src_dir`` end the walk; they live in versioned
    framework/toolchain installs the caller identifies separately.
    Over-approximates (no #ifdef evaluation) — the safe direction for
    cache invalidation.
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
        try:
            data = (src_dir / rel).read_bytes()
        except OSError as err:
            # Hash a marker so an unreadable header invalidates instead of
            # silently vanishing from the digest
            _LOGGER.warning("Could not read %s for the pch checksum: %s", rel, err)
            data = b"<unreadable>"
        seen[rel] = data
        parent = posixpath.dirname(rel)
        stack.extend((inc.decode(), parent) for inc in _INCLUDE_RE.findall(data))
    return seen


def pch_checksum(
    src_dir: Path, include_headers: Iterable[str], extra: Iterable[str]
) -> str:
    """Digest standing in for the .gch in ccache's hash: the include closure
    of the prefix header plus caller-supplied identity strings (versioned
    install paths, flags)."""
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
