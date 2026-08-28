"""Shared precompiled-header policy for the build backends.

The prefix either mirrors the TUs' own force-includes (ESP8266) or is a
curated core-header set (ESP-IDF). ``esphome: includes:`` sources receive
it too; Arduino.h visibility there is intended (esphome#8693).
"""

from __future__ import annotations

from collections.abc import Callable, Iterable
from contextlib import suppress
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

# Guarded curated-prefix wrapper for PlatformIO backends without framework
# force-includes (host, esp32); folded by the pch script via build_src_flags.
PCH_PREFIX_HEADER = "esphome/core/pch_prefix.h"

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
    """CI knob: ``ESPHOME_PCH_STRICT=1`` turns pch degrade paths fatal.

    A set-but-unrecognized value raises: a typo must not silently turn
    the gate into a no-op that proves nothing.
    """
    return parse_enable_env("ESPHOME_PCH_STRICT", strict=True) is True


def pch_degraded(reason: str) -> None:
    """Every degrade path funnels through here; strict mode raises."""
    if pch_strict():
        from esphome.core import EsphomeError

        raise EsphomeError(f"ESPHOME_PCH_STRICT: {reason}")


def pch_disabled_degraded() -> None:
    """Strict CI must not read "no pch at all" as success."""
    pch_degraded("pch disabled by ESPHOME_PCH_ENABLE")


def pch_probe_tail(source: str = "-") -> list[str]:
    """The syntax-only compile shared by the probe and its baseline."""
    return ["-fsyntax-only", "-x", "c++", source]


def pch_probe_args(header: str, source: str = "-") -> list[str]:
    """Flags that load-check a built .gch via a syntax-only compile.

    Rejection must be a nonzero exit (never just a wording match), so the
    invalid-pch class is always escalated. ``source`` defaults to stdin
    (host independent); the ninja probe edge passes a real file.
    """
    return [
        "-Winvalid-pch",
        "-Werror=invalid-pch",
        "-include",
        header,
        *pch_probe_tail(source),
    ]


def pch_consumer_escalation() -> str:
    """Consumer-side invalid-pch flag: strict reds the build on rejection
    (per-process, so the probe alone cannot prove the consumers)."""
    return "-Werror=invalid-pch" if pch_strict() else "-Wno-error=invalid-pch"


def pch_cmake_consumer(target: str, sources_var: str) -> str:
    """Emit the CMake block making ``target``'s C++ sources consume the
    pch; empty when disabled. OBJECT_DEPENDS is on the header, not the
    .gch (pch-baked headers drop out of TU depfiles); the -include stays
    relative — an absolute path would poison ccache keys."""
    if not pch_enabled():
        return ""
    escalation = pch_consumer_escalation()
    return f"""
# ESPHome precompiled header (see esphome/build_helpers/pch.py).
# The touch keeps OBJECT_DEPENDS satisfiable when the build system itself
# wiped the build dir after the header was written (west --pristine)
if(NOT EXISTS "${{CMAKE_BINARY_DIR}}/{PCH_HEADER_NAME}")
  file(TOUCH "${{CMAKE_BINARY_DIR}}/{PCH_HEADER_NAME}")
endif()
target_compile_options({target} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:-Winvalid-pch>"
    "$<$<COMPILE_LANGUAGE:CXX>:{escalation}>"
    "$<$<COMPILE_LANGUAGE:CXX>:-include>"
    "$<$<COMPILE_LANGUAGE:CXX>:{PCH_HEADER_NAME}>"
)
set_source_files_properties({sources_var} PROPERTIES
    OBJECT_DEPENDS "${{CMAKE_BINARY_DIR}}/{PCH_HEADER_NAME}")
"""


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


def guarded_prepare(build_dir: Path, prepare: Callable[[], None]) -> None:
    """Run a backend's pch preparation; an optional speedup must never
    abort the build. Strict is read first so its own knob error cannot
    mask the real failure; discard_pch raises if a stale .gch survives;
    the header is ensured so OBJECT_DEPENDS stays satisfiable."""
    try:
        prepare()
    except Exception:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        strict = pch_strict()
        discard_pch(build_dir)
        if strict:
            raise
        header = build_dir / PCH_HEADER_NAME
        if not header.exists():
            try:
                header.touch()
            except OSError as err:
                # The coming OBJECT_DEPENDS error would hide the real cause
                _LOGGER.warning("Could not create the pch placeholder: %s", err)
        _LOGGER.warning(
            "Precompiled header setup failed; compiling without it", exc_info=True
        )


def pch_extra_scripts() -> list[str]:
    """The extra_scripts entries a PlatformIO platform registers for the
    pch; empty when disabled (the script itself has no enable check)."""
    if not pch_enabled():
        pch_disabled_degraded()
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
    # CMake may spell paths through a symlink differently than CORE does
    # (macOS /tmp vs /private/tmp), so compare resolved paths
    src_root = Path(CORE.relative_src_path()).resolve()
    entry = next(
        (
            e
            for e in entries
            if isinstance(e, dict)
            and isinstance(e.get("file"), str)
            and e["file"].endswith(CXX_SOURCE_SUFFIXES)
            and Path(e["file"]).resolve().is_relative_to(src_root)
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


def flags_identity(tokens: Iterable[str]) -> str:
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
) -> str | None:
    """The .sum digest naming this exact pch build: include closure, header
    text, backend identity strings, and the normalized compile command or
    flags (``tokens``). None (warned and degraded) when the identity
    cannot be established."""
    try:
        return pch_checksum(
            src_dir,
            include_headers,
            (
                # The closure is sorted, so root order only enters via the text
                pch_header_text(include_headers),
                *extra,
                flags_identity(tokens),
            ),
        )
    except (OSError, UnicodeError) as err:
        # Identity unknown: a stale cache entry must never be served
        _LOGGER.warning(
            "Could not establish the pch identity; compiling without it: %s", err
        )
        pch_degraded(f"identity unknown: {err}")
        return None


def log_pch_in_use() -> None:
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
    .gch must not force a full rebuild every build. A .gch that survives
    an unlink failure would be consumed silently (wrong output, not a
    slow build), so that raises.
    """
    header = build_dir / PCH_HEADER_NAME
    gch = Path(f"{header}.gch")
    had_gch = gch.is_file()
    errors = []
    for sidecar in (gch, Path(f"{gch}.sum")):
        try:
            sidecar.unlink(missing_ok=True)
        except OSError as err:
            if sidecar.is_file():
                from esphome.core import EsphomeError

                raise EsphomeError(
                    f"Could not discard the stale precompiled header: {err}"
                ) from err
            errors.append(err)
    for err in errors:
        _LOGGER.warning("Could not discard the pch sidecars: %s", err)
    if had_gch and header.is_file():
        with suppress(OSError):
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
    checksum = pch_identity(cmd, CORE.relative_src_path(), include_headers, extra)
    if checksum is None:
        discard_pch(build_dir)
        return
    failed_marker = Path(f"{gch}.failed")

    def _run(
        run_cmd: list[str], what: str, stdin: str | None = None
    ) -> subprocess.CompletedProcess | None:
        """Spawn one pch tool step; environmental failures discard and
        degrade (None): spawn/IO/timeout errors and signal kills never
        latch the marker."""
        try:
            proc = subprocess.run(
                run_cmd,
                cwd=cmd_dir,
                # C locale keeps diagnostics matchable by _TRANSIENT_ERRORS
                env={**os.environ, "LC_ALL": "C"},
                input=stdin,
                capture_output=True,
                text=True,
                check=False,
                timeout=300,
            )
        except (OSError, subprocess.SubprocessError) as err:
            _LOGGER.warning("Precompiled header %s did not run: %s", what, err)
            discard_pch(build_dir)
            pch_degraded(f"{what} did not run: {err}")
            return None
        if proc.returncode < 0:
            # Killed by a signal (OOM, ^C): environmental, do not latch
            _LOGGER.warning(
                "Precompiled header %s was killed (signal %d); retrying next build",
                what,
                -proc.returncode,
            )
            discard_pch(build_dir)
            pch_degraded(f"{what} killed by signal {-proc.returncode}")
            return None
        return proc

    def _fail(error: str, reason: str, latch: bool) -> None:
        """Discard and degrade; deterministic failures latch when asked."""
        _LOGGER.warning(
            "Precompiled header failed; compiling without it: %s", error[:400]
        )
        # Latching paths keep the full compiler output recoverable
        _LOGGER.debug("Full pch output: %s", error)
        discard_pch(build_dir)
        if latch and not any(m in error for m in _TRANSIENT_ERRORS):
            # Skip retries until a header/flag/backend-identity/command change
            failed_marker.write_text(checksum + "\n", encoding="utf-8")
            os.utime(header)
        pch_degraded(f"{reason}: {error[:200]}")

    def _probe(latch: bool = True) -> None:
        """Load-check the built .gch: some toolchains build one they then
        refuse to load (per-process ASLR). Dep flags are already stripped
        from cmd, so no -MF is needed; cmd ends with the fixed
        "-x c++-header -c -o" tail. A cached-header rejection may not
        reproduce (per-process), so that caller passes latch=False."""
        if cmd[-6:-4] != ["-x", "c++-header"]:
            # The slice below depends on pch_compile_command's fixed tail
            _LOGGER.warning("Unexpected pch command shape: %s", cmd[-6:])
            discard_pch(build_dir)
            pch_degraded("unexpected pch command shape")
            return
        base = cmd[:-6]
        probe = _run([*base, *pch_probe_args(str(header))], "probe", stdin="")
        if probe is None:
            return
        if probe.returncode != 0:
            # Disambiguate: only blame the pch when the same compile passes
            # without it; a failing baseline is its own (latchable) problem
            baseline = _run([*base, *pch_probe_tail()], "probe baseline", stdin="")
            if baseline is None:
                return
            if baseline.returncode == 0:
                error = probe.stderr.strip() or f"exit code {probe.returncode}"
                _fail(error, "toolchain cannot load the pch", latch=latch)
            else:
                error = baseline.stderr.strip() or f"exit code {baseline.returncode}"
                _fail(error, "probe cannot run at all", latch=latch)

    if gch.is_file() and _read_stamp(sum_path) == checksum:
        log_pch_in_use()
        if pch_strict():
            # Rejection is per-process, so a cached .gch must re-prove
            # loadability for the strict gate (CI-only cost); no latch,
            # since the rejection may not reproduce either
            _probe(latch=False)
        return
    if _read_stamp(failed_marker) == checksum:
        _LOGGER.info(
            "Precompiled header disabled after an earlier failure; delete %s to retry",
            failed_marker,
        )
        pch_degraded("earlier failure latched")
        return
    log_pch_in_use()
    result = _run(cmd, "compile")
    if result is None:
        return
    error = None
    if result.returncode != 0:
        error = result.stderr.strip() or f"exit code {result.returncode}"
    elif not gch.is_file():
        error = "compiler produced no .gch"
    if error is not None:
        _fail(error, "compile failed", latch=True)
        return
    _probe()
    if not gch.is_file():
        # The probe discarded a rejected or unrunnable .gch
        return
    failed_marker.unlink(missing_ok=True)
    sum_path.write_text(checksum + "\n", encoding="utf-8")
    # Consumers depend on the header (depfiles cannot see through a .gch);
    # bump it so users of the previous .gch recompile
    os.utime(header)
