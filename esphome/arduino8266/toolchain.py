"""Native Arduino ESP8266 build driver (the PlatformIO ``run`` equivalent)."""

from __future__ import annotations

import json
import logging
from pathlib import Path
import subprocess

from esphome.arduino8266 import framework
from esphome.const import (
    CONF_COMPILE_PROCESS_LIMIT,
    CONF_ESPHOME,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
)
from esphome.core import CORE, EsphomeError
from esphome.helpers import write_file_if_changed
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

# ESP8266 user RAM (matches upload.maximum_ram_size in every board manifest)
_MAX_RAM_SIZE = 81920


def _warn_ignored_platformio_options() -> None:
    """Warn for component-added platformio options the native build drops.

    The consumed set derives from NATIVE_ARDUINO_PIO_OPTIONS so the two
    lists cannot drift; YAML upload_speed never reaches
    CORE.platformio_options here.
    """
    from esphome.core.config import NATIVE_ARDUINO_PIO_OPTIONS

    consumed = NATIVE_ARDUINO_PIO_OPTIONS | {"lib_ignore"}
    for key in sorted(CORE.platformio_options or {}):
        if key not in consumed:
            _LOGGER.warning(
                "platformio_options->%s is ignored when building with the "
                "native 'arduino' toolchain",
                key,
            )


_RAM_SECTIONS = (".data", ".rodata", ".bss")
_FLASH_SECTIONS = (".irom0.text", ".text", ".text1", ".data", ".rodata")


def get_build_dir() -> Path:
    return CORE.relative_pioenvs_path(CORE.name)


def get_elf_path() -> Path:
    return get_build_dir() / "firmware.elf"


def _toolchain_tool(name: str) -> Path:
    return framework.toolchain_tool(framework.get_toolchain_path(), name)


def get_addr2line_path() -> Path:
    return _toolchain_tool("addr2line")


def get_objdump_path() -> Path:
    return _toolchain_tool("objdump")


def get_readelf_path() -> Path:
    return _toolchain_tool("readelf")


def run_compile(config: ConfigType, verbose: bool) -> int:
    from esphome.build_gen import arduino8266 as build_gen

    _warn_ignored_platformio_options()
    paths = framework.check_and_install(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
    # Resolved once per build: the resolution probes PATH and spawns the
    # runnability check, and three consumers need the same answer
    ccache = framework.ccache_path()
    ninja_changed = build_gen.write_project(paths, ccache)

    build_dir = get_build_dir()
    env = framework.get_build_env(paths.toolchain, ccache)

    # Regenerate the compile DB before the build (a pure function of
    # build.ninja); skip when unchanged.
    if ninja_changed or not (build_dir / "compile_commands.json").is_file():
        _write_compile_commands(paths.ninja, build_dir, env)

    cmd = [str(paths.ninja), "-C", str(build_dir)]
    if verbose:
        cmd.append("-v")
    if jobs := config[CONF_ESPHOME].get(CONF_COMPILE_PROCESS_LIMIT):
        cmd += ["-j", str(jobs)]

    _LOGGER.debug("Running: %s", " ".join(cmd))
    rc = subprocess.run(cmd, env=env, check=False, close_fds=False).returncode
    if rc != 0:
        return rc

    _print_size_summary(build_dir, paths)
    try:
        idedata = get_idedata(ccache)
    except (EsphomeError, LookupError, OSError, RuntimeError, ValueError) as err:
        # Broad on purpose: idedata is a bonus artifact; nothing here may
        # fail a successful build.
        _LOGGER.warning("Could not generate idedata: %s", err)
    else:
        if idedata is None:
            _LOGGER.warning(
                "Could not generate idedata from %s",
                build_dir / "compile_commands.json",
            )
    return 0


def _write_compile_commands(
    ninja_path: Path, build_dir: Path, env: dict[str, str]
) -> None:
    result = subprocess.run(
        [str(ninja_path), "-C", str(build_dir), "-t", "compdb", "c", "cxx", "asm"],
        env=env,
        capture_output=True,
        text=True,
        check=False,
        close_fds=False,
    )
    if result.returncode != 0:
        # Drop any stale database so consumers (IDE integration, clang-tidy,
        # the memory analyzer) can't silently read outdated data.
        (build_dir / "compile_commands.json").unlink(missing_ok=True)
        raise EsphomeError(f"Could not generate compile_commands.json: {result.stderr}")
    try:
        entries = json.loads(result.stdout)
    except ValueError:
        entries = None
    if not entries:
        # compdb exits 0 with [] for unknown rule names; a renamed compile
        # rule must fail the build, not silently strand every consumer
        (build_dir / "compile_commands.json").unlink(missing_ok=True)
        raise EsphomeError(
            "ninja produced an empty compile database; the generator's rule "
            "names no longer match"
        )
    # write_file_if_changed keeps the mtime stable on no-op builds so the
    # idedata cache in get_idedata() stays valid.
    write_file_if_changed(build_dir / "compile_commands.json", result.stdout)


def _parse_app_size(build_dir: Path, paths: framework.InstalledPaths) -> int | None:
    """Read the app flash budget (irom0_0_seg length) from the linker script."""
    from esphome.build_gen.arduino8266 import get_flash_ld_path
    from esphome.components.esp8266.build_surgery import segment_length

    # Warnings, not debug: without the app size the Flash summary line is
    # dropped and CI's memory-impact extraction loses its flash metric.
    ld_path = get_flash_ld_path(build_dir, paths)
    try:
        ld_text = ld_path.read_text(encoding="utf-8")
    except OSError as err:
        _LOGGER.warning("Cannot read linker script for the Flash summary: %s", err)
        return None
    app_size = segment_length(ld_text, "irom0_0_seg")
    if app_size is None:
        _LOGGER.warning("irom0_0_seg not found in %s; skipping Flash summary", ld_path)
        return None
    if app_size == 0:
        _LOGGER.warning(
            "irom0_0_seg has zero length in %s; skipping Flash summary", ld_path
        )
        return None
    return app_size


def _print_size_summary(build_dir: Path, paths: framework.InstalledPaths) -> None:
    """Print the PlatformIO-shaped RAM/Flash lines.

    The exact shape (including the bar) is parsed by
    ``script/ci_memory_impact_extract.py``; ``print_size_line`` matches it.
    """
    from esphome.build_helpers.size_summary import print_size_line

    size_tool = _toolchain_tool("size")
    try:
        result = subprocess.run(
            [str(size_tool), "-A", "-d", str(get_elf_path())],
            capture_output=True,
            text=True,
            check=False,
            close_fds=False,
        )
    except OSError as err:
        # The summary is a bonus artifact like idedata; a truncated
        # toolchain extraction must not discard an already-linked build
        _LOGGER.warning("Could not summarize firmware size: %s", err)
        return
    if result.returncode != 0:
        _LOGGER.warning("Could not summarize firmware size: %s", result.stderr)
        return
    sections: dict[str, int] = {}
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0].startswith("."):
            try:
                sections[parts[0]] = int(parts[1])
            except ValueError:
                # An unparsed RAM/Flash section trips the missing-sections
                # guard below, so no total is built on a dropped value
                _LOGGER.warning("Unparsable size output for section %s", parts[0])
    if missing := set(_RAM_SECTIONS + _FLASH_SECTIONS) - set(sections):
        # A defaulted 0 would print a confidently wrong total for CI's metric
        _LOGGER.warning(
            "Size output is missing section(s) %s; skipping the size summary",
            ", ".join(sorted(missing)),
        )
        return
    # Resolve the flash budget before printing anything: a RAM line without
    # its Flash line would let CI's memory-impact extraction sum the two
    # metrics over different build counts (_parse_app_size already warned).
    app_size = _parse_app_size(build_dir, paths)
    if not app_size:
        return
    ram = sum(sections[s] for s in _RAM_SECTIONS)
    flash = sum(sections[s] for s in _FLASH_SECTIONS)
    print_size_line("RAM", ram, _MAX_RAM_SIZE)
    print_size_line("Flash", flash, app_size)


def get_idedata(ccache: str | None = framework.CCACHE_UNRESOLVED) -> dict | None:
    """Derive idedata from the build's compile_commands.json.

    Same contract as ``espidf.toolchain.get_idedata``: the fields IDE
    integrations, clang-tidy, and the memory analyzer expect.
    """
    from esphome.build_helpers.idedata import load_or_build_idedata

    if ccache is framework.CCACHE_UNRESOLVED:
        ccache = framework.ccache_path()
    return load_or_build_idedata(
        get_build_dir() / "compile_commands.json",
        get_elf_path(),
        # Suffixed so a platformio->arduino->platformio round trip on one
        # config never serves the other toolchain's cache shape
        CORE.relative_internal_path("idedata", f"{CORE.name}.arduino.json"),
        # The compile DB's commands carry the same ccache prefix the ninja
        # rules were generated with
        launcher=str(ccache) if ccache else None,
    )
