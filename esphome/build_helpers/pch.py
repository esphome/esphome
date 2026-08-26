"""Shared precompiled-header policy for the build backends.

Safe by construction when the prefix header mirrors what the TUs already
include first (ESP8266); a backend may instead inject a curated set of
self-contained core headers (ESP-IDF). User sources from ``esphome:
includes:`` also receive the prefix, so they now see defines.h (and
Arduino.h on Arduino platforms) even when they did not include it.
"""

from __future__ import annotations

from collections.abc import Iterable
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
    CXX_SOURCE_SUFFIXES,
    expand_response_files,
    is_launcher,
    split_command,
)

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

# Prefix-header contents for backends that inject a curated set (rather
# than mirroring the TUs' own force-includes), defines.h first so USE_*
# macros exist for the rest. Deliberately hard-coded: frequency-derived
# sets measured no better and kept selecting headers that cannot compile
# standalone (X-macro, platform-variant). Every entry must be safe to
# include first in an empty TU. Caveat: application.h/automation.h become
# ambiently visible, so a TU missing those #includes still builds on such
# backends; ESPHOME_PCH_ENABLE=0 restores the strict view.
PCH_DEFAULT_HEADERS = (
    PCH_CORE_HEADER,
    "esphome/core/component.h",
    "esphome/core/helpers.h",
    "esphome/core/log.h",
    "esphome/core/application.h",
    "esphome/core/automation.h",
)

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
    user_sloppiness = os.environ.get("CCACHE_SLOPPINESS")
    if user_sloppiness is not None and "pch_defines" not in user_sloppiness:
        # EXTSUM without pch_defines makes ccache silently decline every
        # pch-consuming compile
        _LOGGER.warning(
            "CCACHE_SLOPPINESS lacks pch_defines; ccache will not cache "
            "compiles that use the precompiled header"
        )
    return {k: v for k, v in _CCACHE_PCH_ENV.items() if k not in os.environ}


def pch_extra_scripts() -> list[str]:
    """The extra_scripts entries a PlatformIO platform registers for the
    pch; empty when disabled (the script itself has no enable check)."""
    return ["post:pch.py"] if pch_enabled() else []


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
            # mtime/size keep a changed-but-unreadable header shifting the
            # digest without device paths in it
            _LOGGER.warning("Could not read %s for the pch checksum: %s", rel, err)
            try:
                st = (src_dir / rel).stat()
                data = f"<unreadable:{st.st_mtime_ns}:{st.st_size}>".encode()
            except OSError:
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


# Compile-command tokens dropped when retargeting a TU's flags at the
# prefix header: source/output/depfile flags with an argument, and the
# argument-less depfile flags (the pch compile must not touch depfiles)
_PCH_STRIP_FLAGS_WITH_ARG = frozenset({"-o", "-c", "-MT", "-MF", "-MQ"})
_PCH_STRIP_FLAGS = frozenset({"-MD", "-MMD", "-MP", "-MM", "-M"})


def pch_compile_command(
    build_dir: Path, header: Path, gch: Path
) -> tuple[list[str], Path] | None:
    """The exact src C++ flags from compile_commands.json retargeted at the
    header, with the directory they resolve against (relative -I paths must
    be expanded and executed from the same root); None (logged) when no
    configured C++ TU is available yet."""
    from esphome.core import CORE

    try:
        entries = json.loads(
            (build_dir / "compile_commands.json").read_text(encoding="utf-8")
        )
    except (OSError, json.JSONDecodeError) as err:
        # Configure already succeeded, so an unusable DB is a real anomaly
        _LOGGER.warning("No usable compile database, skipping pch: %s", err)
        return None
    if not isinstance(entries, list):
        _LOGGER.warning("Malformed compile database, skipping pch")
        return None
    # Windows compile DBs use backslashes; normalize both sides
    src_prefix = str(CORE.relative_src_path()).replace("\\", "/")
    entry = next(
        (
            e
            for e in entries
            if isinstance(e, dict)
            and e.get("file", "").replace("\\", "/").startswith(src_prefix)
            and e.get("file", "").endswith(CXX_SOURCE_SUFFIXES)
        ),
        None,
    )
    if entry is None:
        _LOGGER.warning("No src C++ entry in the compile database, skipping pch")
        return None
    cmd_dir = Path(entry.get("directory", build_dir))
    tokens = expand_response_files(split_command(entry.get("command", "")), cmd_dir)
    # A DB recorded with ccache enabled prefixes the compiler with the
    # launcher; the .gch must be compiled directly
    if tokens and is_launcher(tokens[0]):
        tokens = tokens[1:]
    if not tokens:
        # An "arguments"-style or empty entry must skip cleanly, not spawn
        # a compiler-less argv that warns on every build
        _LOGGER.warning("Compile database entry has no usable command, skipping pch")
        return None
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


def discard_pch(build_dir: Path) -> None:
    """Remove the pch sidecars so a stale .gch is never consumed.

    Bumps the header only when a .gch was actually removed: TUs compiled
    against it have incomplete depfiles, while a repeat failure with no
    .gch must not force a full rebuild every build.
    """
    header = build_dir / PCH_HEADER_NAME
    gch = Path(f"{header}.gch")
    had_gch = gch.is_file()
    gch.unlink(missing_ok=True)
    Path(f"{gch}.sum").unlink(missing_ok=True)
    if had_gch and header.is_file():
        os.utime(header)


def prepare_pch(
    build_dir: Path, include_headers: tuple[str, ...], extra: Iterable[str]
) -> None:
    """Compile ``build_dir``'s .gch from compile_commands.json flags and
    write its ccache .sum.

    The .sum doubles as the freshness stamp and folds in the compile
    command, so a flag-only change rebuilds the .gch; ``extra`` carries
    backend identity (framework version, sdkconfig, ...). A failed
    compile falls back to the plain header include.
    """
    from esphome.core import CORE

    _LOGGER.info(
        "Compiling with a precompiled header (set ESPHOME_PCH_ENABLE=0 to disable)"
    )
    header = build_dir / PCH_HEADER_NAME
    gch = Path(f"{header}.gch")
    sum_path = Path(f"{gch}.sum")
    cmd_and_dir = pch_compile_command(build_dir, header, gch)
    if cmd_and_dir is None:
        # Freshness cannot be validated; a leftover .gch must not be consumed
        discard_pch(build_dir)
        return
    cmd, cmd_dir = cmd_and_dir
    # Stripped like ccache's own rewriting (a user CCACHE_BASEDIR wins) so
    # identical configs hash identically across devices; the raw build path
    # covers unresolved spellings in the compile DB
    cmd_id = (
        " ".join(cmd)
        .replace(effective_ccache_basedir(), "")
        .replace(str(CORE.build_path), "")
    )
    checksum = pch_checksum(
        CORE.relative_src_path(),
        include_headers,
        (
            # The closure is sorted, so root order only enters via the text
            pch_header_text(include_headers),
            *extra,
            cmd_id,
        ),
    )
    if (
        gch.is_file()
        and sum_path.is_file()
        and sum_path.read_text(encoding="utf-8").strip() == checksum
    ):
        return
    failed_marker = Path(f"{gch}.failed")
    if (
        failed_marker.is_file()
        and failed_marker.read_text(encoding="utf-8").strip() == checksum
    ):
        _LOGGER.info(
            "Precompiled header disabled after an earlier failure; delete %s to retry",
            failed_marker,
        )
        return
    try:
        result = subprocess.run(
            cmd, cwd=cmd_dir, capture_output=True, text=True, check=False, timeout=300
        )
        error = None
        if result.returncode != 0:
            error = result.stderr.strip() or f"exit code {result.returncode}"
        elif not gch.is_file():
            error = "compiler produced no .gch"
    except (OSError, subprocess.SubprocessError) as err:
        # Transient (timeout, spawn/IO): warn and retry next build, no marker
        _LOGGER.warning("Precompiled header compile did not run: %s", err)
        discard_pch(build_dir)
        return
    if error is not None:
        _LOGGER.warning(
            "Precompiled header failed; compiling without it: %s", error[:400]
        )
        discard_pch(build_dir)
        # Skip retries until a header/flag/backend-identity/command change
        failed_marker.write_text(checksum + "\n", encoding="utf-8")
        os.utime(header)
        return
    failed_marker.unlink(missing_ok=True)
    sum_path.write_text(checksum + "\n", encoding="utf-8")
    # Consumers depend on the header (depfiles cannot see through a .gch);
    # bump it so users of the previous .gch recompile
    os.utime(header)
