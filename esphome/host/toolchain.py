"""Native host build driver (the PlatformIO ``run`` equivalent).

The compiler and binutils come from PATH (``CC``/``CXX``/``AR``/``OBJDUMP``/
``READELF`` override the lookup, like make and CMake), ninja from PATH or
the ninja PyPI wheel, and ccache is used when found. The build lives under
``.pioenvs/<name>/`` so ``CORE.firmware_bin`` and the clean paths stay the
ones the PlatformIO build used.
"""

from __future__ import annotations

import logging
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from typing import NamedTuple

from esphome.build_helpers.ccache import ccache_env, resolve_absolute_ccache_path
from esphome.build_helpers.native import warn_ignored_platformio_options
from esphome.build_helpers.ninja import (
    absolute_tool,
    find_ninja,
    refresh_compile_commands,
)
from esphome.build_helpers.tools_cache import HOST_TOOLS_CACHE
from esphome.const import CONF_COMPILE_PROCESS_LIMIT, CONF_ESPHOME
from esphome.core import CORE, EsphomeError
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

# The output name PlatformIO's native platform produced; CORE.firmware_bin
# and the integration-test harness resolve it by this name
PROGRAM_NAME = "program"

# platformio_options keys the host build reads (lib_ignore feeds the library
# converter); anything else has no native equivalent and is warned about
CONSUMED_PIO_OPTIONS = frozenset({"lib_ignore"})


class HostCompilers(NamedTuple):
    """The resolved C and C++ compiler commands: the program, then any
    arguments its override carried (``CC="gcc -m32"``)."""

    cc: tuple[str, ...]
    cxx: tuple[str, ...]


def find_command(env_var: str, candidates: tuple[str, ...]) -> tuple[str, ...]:
    """Resolve a build tool: ``env_var`` when set, else the first candidate
    found on PATH. Returns the program and the override's arguments.

    An override that does not resolve fails by name rather than falling back
    silently to a different compiler than the user asked for.
    """
    if override := os.environ.get(env_var, "").strip():
        program, *args = shlex.split(override)
        # which() accepts an absolute path as well as a bare program name
        if (resolved := shutil.which(program)) is None:
            raise EsphomeError(
                f"{env_var}={override!r} does not name a runnable program"
            )
        return (absolute_tool(resolved), *args)
    for name in candidates:
        if (found := shutil.which(name)) is not None:
            return (absolute_tool(found),)
    raise EsphomeError(
        f"{candidates[0]} not found on PATH (tried {', '.join(candidates)}); "
        f"install it or set {env_var}"
    )


def find_tool(env_var: str, candidates: tuple[str, ...]) -> str:
    """Resolve a build tool that is called as a bare program."""
    program, *args = find_command(env_var, candidates)
    if args:
        raise EsphomeError(f"{env_var} must name a program without arguments")
    return program


def find_compilers() -> HostCompilers:
    """The C and C++ compilers the build uses (gcc first, as PlatformIO did)."""
    return HostCompilers(
        cc=find_command("CC", ("gcc", "clang", "cc")),
        cxx=find_command("CXX", ("g++", "clang++", "c++")),
    )


def get_build_dir() -> Path:
    return CORE.relative_pioenvs_path(CORE.name)


def get_elf_path() -> Path:
    return get_build_dir() / PROGRAM_NAME


def get_objdump_path() -> Path:
    return Path(find_tool("OBJDUMP", ("objdump",)))


def get_readelf_path() -> Path:
    return Path(find_tool("READELF", ("readelf",)))


def check_analysis_supported() -> None:
    """Refuse analyze-memory where the program is not an ELF file.

    Called before the compile, so an unsupported machine fails at once.
    """
    if sys.platform != "linux":
        raise EsphomeError(
            "analyze-memory reads ELF files; the host build on "
            f"{sys.platform} produces a different format"
        )


def get_build_env(ccache: str | None) -> dict[str, str]:
    return {**os.environ, **ccache_env(ccache, HOST_TOOLS_CACHE)}


def run_compile(config: ConfigType, verbose: bool) -> int:
    from esphome.build_gen import host as build_gen

    warn_ignored_platformio_options(CONSUMED_PIO_OPTIONS)
    # Probe the cheap local dependencies before resolving libraries
    ninja_path = find_ninja()
    compilers = find_compilers()
    # Resolved once per build: the resolution probes PATH and spawns the
    # runnability check, and three consumers need the same answer
    ccache = resolve_absolute_ccache_path()
    ninja_changed = build_gen.write_project(compilers, ccache)

    build_dir = get_build_dir()
    env = get_build_env(ccache)
    refresh_compile_commands(ninja_path, build_dir, env, ninja_changed)

    cmd = [str(ninja_path)]
    if verbose:
        cmd.append("-v")
    if jobs := config[CONF_ESPHOME].get(CONF_COMPILE_PROCESS_LIMIT):
        cmd += ["-j", str(jobs)]
    # The explicit target, not the default statement: a generator defect
    # that drops it fails loudly with "unknown target" instead of a green
    # no-op run that leaves a stale program in place
    cmd.append(PROGRAM_NAME)

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

    warn_if_idedata_missing(lambda: _load_idedata(ccache))
    return 0


def get_idedata() -> dict | None:
    """Derive idedata from the build's compile_commands.json.

    Same contract as ``espidf.toolchain.get_idedata``: the fields IDE
    integrations, clang-tidy, and the memory analyzer expect. Returns None
    when nothing has been built yet.
    """
    # Deliberately uncached: env/PATH can change between builds in a
    # long-lived host process
    return _load_idedata(resolve_absolute_ccache_path())


def _load_idedata(ccache: str | None) -> dict | None:
    from esphome.build_helpers.idedata import load_or_build_idedata

    return load_or_build_idedata(
        get_build_dir() / "compile_commands.json",
        get_elf_path(),
        CORE.relative_internal_path("idedata", f"{CORE.name}.json"),
        # The compile DB's commands carry the same ccache prefix the ninja
        # rules were generated with
        launcher=ccache,
    )
