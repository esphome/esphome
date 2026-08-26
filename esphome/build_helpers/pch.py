"""Shared precompiled-header policy for the build backends.

The prefix either mirrors the TUs' own force-includes (ESP8266) or is a
curated core-header set (ESP-IDF). ``esphome: includes:`` sources receive
it too; Arduino.h visibility there is intended (esphome#8693).
"""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass
import hashlib
import logging
import os
from pathlib import Path
import posixpath
import re

from esphome.build_helpers.ccache import parse_enable_env

_DOMAIN = "pch"


@dataclass
class _PCHData:
    emitted: bool = False


def _pch_data() -> _PCHData:
    from esphome.core import CORE

    if _DOMAIN not in CORE.data:
        CORE.data[_DOMAIN] = _PCHData()
    return CORE.data[_DOMAIN]


def mark_pch_emitted() -> None:
    """Record that this build's consumers reference the pch."""
    _pch_data().emitted = True


_LOGGER = logging.getLogger(__name__)

# The header and its .gch/.sum sidecars live in the build directory.
PCH_HEADER_NAME = "esphome_pch.h"

# Every artifact the pch machinery can leave behind, for cleanup.
PCH_ARTIFACT_NAMES = (
    PCH_HEADER_NAME,
    f"{PCH_HEADER_NAME}.gch",
    f"{PCH_HEADER_NAME}.gch.sum",
    f"{PCH_HEADER_NAME}.gch.failed",
)

# The core defines header every backend anchors its prefix on.
PCH_CORE_HEADER = "esphome/core/defines.h"

# ccache cannot hash through a .gch; CCACHE_PCH_EXTSUM makes it hash the
# .sum sidecar instead of the .gch bytes, which are not reproducible.
# Keep in sync with the literals in platformio/pch.py.script.
_CCACHE_PCH_ENV = {
    "CCACHE_SLOPPINESS": "pch_defines,time_macros",
    "CCACHE_PCH_EXTSUM": "true",
}

# Both include forms: an angle include resolving under src/ must enter the
# digest too; ones that do not resolve simply end the walk
_INCLUDE_RE = re.compile(rb'^\s*#\s*include\s+["<]([^">]+)[">]', re.MULTILINE)


def pch_enabled() -> bool:
    """Precompiled-header knob: default on, ``ESPHOME_PCH_ENABLE=0`` opts out."""
    return parse_enable_env("ESPHOME_PCH_ENABLE") is not False


def ccache_pch_env() -> dict[str, str]:
    """Settings ccache needs to cache compiles that consume the .gch;
    empty unless this build actually emitted one. User-set values win.
    Native backends export these process-wide; only time_macros affects
    non-pch TUs."""
    if not (pch_enabled() and _pch_data().emitted):
        return {}
    env = {k: v for k, v in _CCACHE_PCH_ENV.items() if k not in os.environ}
    user_sloppiness = os.environ.get("CCACHE_SLOPPINESS")
    if user_sloppiness is not None and (
        missing := [
            t
            for t in ("pch_defines", "time_macros")
            if t not in {tok.strip() for tok in user_sloppiness.split(",")}
        ]
    ):
        # Without these ccache declines every pch-consuming compile
        env["CCACHE_SLOPPINESS"] = ",".join((user_sloppiness, *missing))
        _LOGGER.warning(
            "Adding %s to CCACHE_SLOPPINESS so ccache can cache compiles "
            "that use the precompiled header",
            ",".join(missing),
        )
    return env


def pch_extra_scripts() -> list[str]:
    """The extra_scripts entries a PlatformIO platform registers for the
    pch; empty when disabled (the script itself has no enable check)."""
    return ["post:pch.py"] if pch_enabled() else []


def pch_header_text(include_headers: Iterable[str]) -> str:
    """The prefix-header source: exactly these includes, in order."""
    return "".join(f'#include "{name}"\n' for name in include_headers)


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
            if not rel.startswith("..") and (src_dir / rel).is_file():
                break
        else:
            continue
        if rel in seen:
            continue
        try:
            data = (src_dir / rel).read_bytes()
        except OSError as err:
            # mtime/size still shift the digest; a stat failure propagates
            # so callers compile without a pch
            _LOGGER.warning("Could not read %s for the pch checksum: %s", rel, err)
            st = (src_dir / rel).stat()
            data = f"<unreadable:{st.st_mtime_ns}:{st.st_size}>".encode()
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
