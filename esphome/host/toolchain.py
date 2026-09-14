"""Native host build driver (the PlatformIO ``run`` equivalent).

The compiler and binutils come from PATH (``CC``/``CXX``/``AR``/``OBJDUMP``/
``READELF`` override the lookup, like make and CMake), ninja from PATH or
the ninja PyPI wheel, and ccache is used when found. The build lives under
``.pioenvs/<name>/`` so ``CORE.firmware_bin`` and the clean paths stay the
ones the PlatformIO build used.
"""

from __future__ import annotations

import json
import logging
import os
from pathlib import Path
import shutil
import subprocess
from typing import Any, NamedTuple

from esphome.build_helpers.ccache import ccache_defaults_env, resolve_ccache_path
from esphome.build_helpers.ninja import find_ninja
from esphome.build_helpers.tools_cache import HOST_TOOLS_CACHE, tools_cache_path
from esphome.const import CONF_COMPILE_PROCESS_LIMIT, CONF_ESPHOME
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import strip_win_long_path_prefix
from esphome.helpers import write_file_if_changed
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

# The output name PlatformIO's native platform produced; CORE.firmware_bin
# and the integration-test harness resolve it by this name
PROGRAM_NAME = "program"

# platformio_options keys the host build reads (lib_ignore feeds the library
# converter); anything else has no native equivalent and is warned about
CONSUMED_PIO_OPTIONS = frozenset({"lib_ignore"})

# Compile rule names the generator emits; ninja's compdb tool is asked for
# exactly these, so a renamed rule fails the build instead of stranding idedata
COMPILE_RULES = ("c", "cxx", "aspp", "asm")


class HostCompilers(NamedTuple):
    """The resolved C and C++ compiler paths."""

    cc: str
    cxx: str


def find_tool(env_var: str, candidates: tuple[str, ...]) -> str:
    """Resolve a build tool: ``env_var`` when set, else the first candidate
    found on PATH.

    An override that does not resolve fails by name rather than falling back
    silently to a different compiler than the user asked for.
    """
    if override := os.environ.get(env_var, "").strip():
        # which() accepts an absolute path as well as a bare program name
        resolved = shutil.which(override)
        if resolved is None:
            raise EsphomeError(
                f"{env_var}={override!r} does not name a runnable program"
            )
        return strip_win_long_path_prefix(resolved)
    for name in candidates:
        if (found := shutil.which(name)) is not None:
            return strip_win_long_path_prefix(found)
    raise EsphomeError(
        f"{candidates[0]} not found on PATH (tried {', '.join(candidates)}); "
        f"install it or set {env_var}"
    )


def find_compilers() -> HostCompilers:
    """The C and C++ compilers the build uses (gcc first, as PlatformIO did)."""
    return HostCompilers(
        cc=find_tool("CC", ("gcc", "clang", "cc")),
        cxx=find_tool("CXX", ("g++", "clang++", "c++")),
    )


def get_build_dir() -> Path:
    return CORE.relative_pioenvs_path(CORE.name)


def get_elf_path() -> Path:
    return get_build_dir() / PROGRAM_NAME


def get_objdump_path() -> Path:
    return Path(find_tool("OBJDUMP", ("objdump",)))


def get_readelf_path() -> Path:
    return Path(find_tool("READELF", ("readelf",)))


def ccache_env(ccache: str | None) -> dict[str, str]:
    """The ccache settings for the build subprocess (not os.environ).

    ``ccache`` is the pre-resolved binary (resolve_ccache_path), or None when
    disabled. Values the user already set in the environment are respected.
    """
    if ccache is None:
        return {}
    return ccache_defaults_env(tools_cache_path(*HOST_TOOLS_CACHE) / "ccache")


def get_build_env(ccache: str | None) -> dict[str, str]:
    env = os.environ.copy()
    env.update(ccache_env(ccache))
    return env


def _warn_ignored_platformio_options() -> None:
    """Warn for component-added platformio options the native build drops.

    User-supplied keys were already routed or warned about by
    ``core/config.py``; what survives into ``CORE.platformio_options`` came
    from ``cg.add_platformio_option`` calls in components.
    """
    for key in sorted(CORE.platformio_options or {}):
        if key not in CONSUMED_PIO_OPTIONS:
            _LOGGER.warning(
                "platformio_options->%s is ignored when building with the "
                "native 'host' toolchain",
                key,
            )


def run_compile(config: ConfigType, verbose: bool) -> int:
    from esphome.build_gen import host as build_gen

    _warn_ignored_platformio_options()
    # Probe the cheap local dependencies before resolving libraries
    ninja_path = find_ninja()
    compilers = find_compilers()
    # Resolved once per build: the resolution probes PATH and spawns the
    # runnability check, and three consumers need the same answer
    ccache = resolve_ccache_path()
    ninja_changed = build_gen.write_project(compilers, ccache)

    build_dir = get_build_dir()
    env = get_build_env(ccache)
    _refresh_compile_commands(ninja_path, build_dir, env, ninja_changed)

    cmd = [str(ninja_path)]
    if verbose:
        cmd.append("-v")
    if jobs := config[CONF_ESPHOME].get(CONF_COMPILE_PROCESS_LIMIT):
        cmd += ["-j", str(jobs)]
    # The explicit target, not the default statement: a generator defect
    # that drops it fails loudly with "unknown target" instead of a green
    # no-op run that leaves a stale program in place
    cmd.append(PROGRAM_NAME)

    # A freshly rewritten manifest all but guarantees work, so the dry-run
    # probe (and its stat pass) only runs on an unchanged one
    if not ninja_changed and _nothing_to_do(ninja_path, build_dir, env):
        _LOGGER.debug("ninja: nothing to rebuild")
    else:
        _LOGGER.debug("Running: %s", " ".join(cmd))
        # cwd instead of -C also drops the "Entering directory" banner
        rc = subprocess.run(
            cmd, cwd=build_dir, env=env, check=False, close_fds=False
        ).returncode
        if rc != 0:
            return rc

    elf = get_elf_path()
    if not elf.is_file():
        # ninja refused a manifest missing the target above; this covers a
        # rule that ran but wrote elsewhere
        _LOGGER.error("Build produced no %s", elf)
        return 1

    from esphome.build_helpers.idedata import warn_if_idedata_missing

    warn_if_idedata_missing(lambda: get_idedata(ccache))
    return 0


def _nothing_to_do(ninja_path: Path, build_dir: Path, env: dict[str, str]) -> bool:
    """Whether a dry run reports the program up to date.

    Keeps a no-op rebuild quiet: ninja would only print "no work to do".
    """
    probe = subprocess.run(
        [str(ninja_path), "-n", PROGRAM_NAME],
        cwd=build_dir,
        env=env,
        capture_output=True,
        text=True,
        check=False,
        close_fds=False,
    )
    if probe.stderr.strip():
        # A load-time diagnostic (e.g. "multiple rules generate X") flags a
        # generator bug; the skip branch would otherwise swallow it forever
        _LOGGER.warning("ninja: %s", probe.stderr.strip())
    if probe.returncode != 0:
        # An unknown target here is the defective-manifest case; the real
        # build prints the error attributably
        _LOGGER.debug("ninja probe failed; running the full build")
        return False
    return "no work to do" in probe.stdout


def _refresh_compile_commands(
    ninja_path: Path, build_dir: Path, env: dict[str, str], ninja_changed: bool
) -> None:
    """Regenerate the compile DB (a pure function of build.ninja) when stale.

    Freshness rides a stamp: the DB itself is written through
    write_file_if_changed (its mtime feeds get_idedata's cache), so a
    regeneration with identical content would stay "stale" forever. An
    interrupted previous run may have rewritten the manifest without
    regenerating the DB, hence the mtime comparison.
    """
    compdb = build_dir / "compile_commands.json"
    compdb_stamp = build_dir / ".compile_commands.stamp"
    ninja_file = build_dir / "build.ninja"
    if (
        ninja_changed
        or not compdb.is_file()
        or not compdb_stamp.is_file()
        or compdb_stamp.stat().st_mtime < ninja_file.stat().st_mtime
    ):
        _write_compile_commands(ninja_path, build_dir, env)
        compdb_stamp.touch()


def _write_compile_commands(
    ninja_path: Path, build_dir: Path, env: dict[str, str]
) -> None:
    compdb = build_dir / "compile_commands.json"
    result = subprocess.run(
        [str(ninja_path), "-C", str(build_dir), "-t", "compdb", *COMPILE_RULES],
        env=env,
        capture_output=True,
        text=True,
        check=False,
        close_fds=False,
    )
    if result.returncode != 0:
        # Drop any stale database so consumers (IDE integration, clang-tidy,
        # the memory analyzer) can't silently read outdated data
        compdb.unlink(missing_ok=True)
        raise EsphomeError(f"Could not generate compile_commands.json: {result.stderr}")
    try:
        entries = json.loads(result.stdout)
    except ValueError as err:
        compdb.unlink(missing_ok=True)
        raise EsphomeError(
            f"ninja produced an unparsable compile database: {err} "
            f"(output starts {result.stdout[:120]!r})"
        ) from err
    if not entries:
        # compdb exits 0 with [] for unknown rule names; a renamed compile
        # rule must fail the build, not silently strand every consumer
        compdb.unlink(missing_ok=True)
        raise EsphomeError(
            "ninja produced an empty compile database; the generator's rule "
            "names no longer match"
        )
    # write_file_if_changed keeps the mtime stable on no-op builds so the
    # idedata cache in get_idedata() stays valid
    write_file_if_changed(compdb, result.stdout)


# Sentinel: "resolve for me"; None is a real value meaning disabled.
_CCACHE_UNRESOLVED: Any = object()


def get_idedata(ccache: str | None = _CCACHE_UNRESOLVED) -> dict | None:
    """Derive idedata from the build's compile_commands.json.

    Same contract as ``espidf.toolchain.get_idedata``: the fields IDE
    integrations, clang-tidy, and the memory analyzer expect. Returns None
    when nothing has been built yet.
    """
    from esphome.build_helpers.idedata import load_or_build_idedata

    if ccache is _CCACHE_UNRESOLVED:
        # Deliberately uncached: env/PATH can change between builds in a
        # long-lived host process
        ccache = resolve_ccache_path()
    return load_or_build_idedata(
        get_build_dir() / "compile_commands.json",
        get_elf_path(),
        CORE.relative_internal_path("idedata", f"{CORE.name}.json"),
        # The compile DB's commands carry the same ccache prefix the ninja
        # rules were generated with
        launcher=str(ccache) if ccache else None,
    )
