"""Shared precompiled-header policy for the build backends.

The prefix either mirrors the TUs' own force-includes (ESP8266) or is a
curated core-header set (ESP-IDF). ``esphome: includes:`` sources receive
it too; Arduino.h visibility there is intended (esphome#8693).
"""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass
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

# Both include forms: an angle include resolving under src/ must enter the
# digest too; ones that do not resolve simply end the walk
# Compiler failures that clear on their own must not latch the .failed marker
_TRANSIENT_ERRORS = ("No space left", "Cannot allocate", "Resource temporarily")

_INCLUDE_RE = re.compile(rb'^\s*#\s*include\s+["<]([^">]+)[">]', re.MULTILINE)


def pch_enabled() -> bool:
    """Precompiled-header knob: default on, ``ESPHOME_PCH_ENABLE=0`` opts out."""
    return parse_enable_env("ESPHOME_PCH_ENABLE") is not False


def pch_strict() -> bool:
    """CI knob: ``ESPHOME_PCH_STRICT=1`` turns pch degrade paths fatal."""
    return parse_enable_env("ESPHOME_PCH_STRICT") is True


def pch_degraded(reason: str) -> None:
    """Every degrade path funnels through here; strict mode raises."""
    if pch_strict():
        from esphome.core import EsphomeError

        raise EsphomeError(f"ESPHOME_PCH_STRICT: {reason}")


def ccache_pch_env() -> dict[str, str]:
    """Settings ccache needs to cache compiles that consume the .gch;
    empty unless this build actually emitted one. User-set values win.
    Native backends export these process-wide; only time_macros affects
    non-pch TUs."""
    if not (pch_enabled() and _pch_data().emitted):
        return {}
    extsum = os.environ.get("CCACHE_PCH_EXTSUM")
    if extsum is not None and extsum.strip().lower() not in ("1", "true", "yes", "on"):
        # ccache then hashes the non-reproducible .gch bytes: permanent misses
        _LOGGER.warning("CCACHE_PCH_EXTSUM=%s disables pch caching", extsum)
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
    if not pch_enabled():
        # Strict CI must not read "no pch at all" as success
        pch_degraded("pch disabled by ESPHOME_PCH_ENABLE")
        return []
    return ["post:pch.py"]


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
            and isinstance(e.get("file"), str)
            and e["file"].replace("\\", "/").startswith(src_prefix)
            and e["file"].endswith(CXX_SOURCE_SUFFIXES)
        ),
        None,
    )
    if entry is None:
        _LOGGER.warning("No src C++ entry in the compile database, skipping pch")
        return None
    directory = entry.get("directory")
    cmd_dir = Path(directory) if isinstance(directory, str) and directory else build_dir
    command = entry.get("command")
    tokens = expand_response_files(
        split_command(command if isinstance(command, str) else ""), cmd_dir
    )
    # A DB recorded with ccache enabled prefixes the compiler with the
    # launcher; the .gch must be compiled directly
    if tokens and is_launcher(tokens[0]):
        tokens = tokens[1:]
    if not tokens:
        # "arguments"-style or empty entries must skip, not spawn "-x ..."
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


def _log_pch_in_use() -> None:
    # The only place a user can discover the knob; emitted only once a
    # .gch is actually fresh or being built
    _LOGGER.info(
        "Compiling with a precompiled header (set ESPHOME_PCH_ENABLE=0 to disable)"
    )


def _read_stamp(path: Path) -> str:
    """A corrupt sidecar must read as stale, not kill the pch forever."""
    try:
        return path.read_text(encoding="utf-8").strip()
    except (OSError, UnicodeDecodeError):
        return ""


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

    header = build_dir / PCH_HEADER_NAME
    gch = Path(f"{header}.gch")
    sum_path = Path(f"{gch}.sum")
    cmd_and_dir = pch_compile_command(build_dir, header, gch)
    if cmd_and_dir is None:
        # Freshness cannot be validated; a leftover .gch must not be consumed
        discard_pch(build_dir)
        pch_degraded("no usable compile command")
        return
    cmd, cmd_dir = cmd_and_dir
    # Strip like ccache's rewriting (user CCACHE_BASEDIR wins); the raw
    # build path covers unresolved (symlinked) spellings
    cmd_id = (
        " ".join(cmd)
        .replace(effective_ccache_basedir(), "")
        .replace(str(CORE.build_path), "")
    )
    try:
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
    except (OSError, UnicodeError) as err:
        # Identity unknown: a stale cache entry must never be served
        _LOGGER.warning(
            "Could not establish the pch identity; compiling without it: %s", err
        )
        discard_pch(build_dir)
        pch_degraded(f"identity unknown: {err}")
        return
    if gch.is_file() and _read_stamp(sum_path) == checksum:
        _log_pch_in_use()
        return
    failed_marker = Path(f"{gch}.failed")
    if _read_stamp(failed_marker) == checksum:
        _LOGGER.info(
            "Precompiled header disabled after an earlier failure; delete %s to retry",
            failed_marker,
        )
        pch_degraded("earlier failure latched")
        return
    _log_pch_in_use()
    try:
        result = subprocess.run(
            cmd,
            cwd=cmd_dir,
            # C locale keeps diagnostics matchable by _TRANSIENT_ERRORS
            env={**os.environ, "LC_ALL": "C"},
            capture_output=True,
            text=True,
            check=False,
            timeout=300,
        )
        error = None
        if result.returncode < 0:
            # Killed by a signal (OOM, ^C): environmental, do not latch
            _LOGGER.warning(
                "Precompiled header compile was killed (signal %d); retrying "
                "next build",
                -result.returncode,
            )
            discard_pch(build_dir)
            pch_degraded(f"compile killed by signal {-result.returncode}")
            return
        if result.returncode != 0:
            error = result.stderr.strip() or f"exit code {result.returncode}"
        elif not gch.is_file():
            error = "compiler produced no .gch"
    except (OSError, subprocess.SubprocessError) as err:
        # Transient (timeout, spawn/IO): warn and retry next build, no marker
        _LOGGER.warning("Precompiled header compile did not run: %s", err)
        discard_pch(build_dir)
        pch_degraded(f"compile did not run: {err}")
        return
    if error is not None:
        _LOGGER.warning(
            "Precompiled header failed; compiling without it: %s", error[:400]
        )
        # This path latches, so keep the full compiler output recoverable
        _LOGGER.debug("Full pch compile output: %s", error)
        discard_pch(build_dir)
        if any(m in error for m in _TRANSIENT_ERRORS):
            # Resource exhaustion clears on its own; retry next build
            pch_degraded(f"transient compile failure: {error[:200]}")
            return
        # Skip retries until a header/flag/backend-identity/command change
        failed_marker.write_text(checksum + "\n", encoding="utf-8")
        os.utime(header)
        pch_degraded(f"compile failed: {error[:200]}")
        return
    # Load probe: some toolchains build a .gch they then refuse to load
    # (per-process ASLR); dep flags are already stripped from cmd, so no
    # -MF is needed
    probe_cmd = [
        *cmd[: cmd.index("-x")],
        "-Winvalid-pch",
        "-include",
        str(header),
        "-fsyntax-only",
        "-x",
        "c++",
        os.devnull,
    ]
    try:
        probe = subprocess.run(
            probe_cmd,
            cwd=cmd_dir,
            env={**os.environ, "LC_ALL": "C"},
            capture_output=True,
            text=True,
            check=False,
            timeout=300,
        )
    except (OSError, subprocess.SubprocessError) as err:
        _LOGGER.warning("Precompiled header probe did not run: %s", err)
        discard_pch(build_dir)
        pch_degraded(f"probe did not run: {err}")
        return
    if probe.returncode != 0 or ".gch" in probe.stderr:
        error = f"toolchain cannot load the pch: {probe.stderr.strip()[:400]}"
        _LOGGER.warning("Precompiled header failed; compiling without it: %s", error)
        discard_pch(build_dir)
        failed_marker.write_text(checksum + "\n", encoding="utf-8")
        os.utime(header)
        pch_degraded(error)
        return
    failed_marker.unlink(missing_ok=True)
    sum_path.write_text(checksum + "\n", encoding="utf-8")
    # Consumers depend on the header (depfiles cannot see through a .gch);
    # bump it so users of the previous .gch recompile
    os.utime(header)
