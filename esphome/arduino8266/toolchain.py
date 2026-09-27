"""Native Arduino ESP8266 build driver (the PlatformIO ``run`` equivalent)."""

from __future__ import annotations

import json
import logging
from pathlib import Path
import subprocess
from typing import TYPE_CHECKING

from esphome.build_helpers.ccache import resolve_ccache_path
from esphome.const import (
    CONF_COMPILE_PROCESS_LIMIT,
    CONF_ESPHOME,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
)
from esphome.core import CORE, EsphomeError
from esphome.helpers import write_file
from esphome.types import ConfigType

if TYPE_CHECKING:
    from esphome.arduino8266.framework import InstalledPaths

_LOGGER = logging.getLogger(__name__)

# ESP8266 user RAM (matches upload.maximum_ram_size in every board manifest)
_MAX_RAM_SIZE = 81920


def _warn_ignored_platformio_options() -> None:
    """Warn for component-added platformio options the native build drops."""
    from esphome.core.config import NATIVE_ARDUINO_CONSUMED_PIO_OPTIONS

    consumed = NATIVE_ARDUINO_CONSUMED_PIO_OPTIONS
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
    # Imported here, not at module scope: the serial upload/logs fast path
    # resolves this module for its artifact paths alone, and framework
    # pulls in the whole package-download stack
    from esphome.arduino8266 import framework

    return framework.toolchain_tool(framework.get_toolchain_path(), name)


def get_factory_firmware_path() -> Path:
    """The image to serial-flash at 0x0 (same bytes as firmware.bin: the
    8266 factory copy exists for artifact-contract parity, not content)."""
    return get_build_dir() / "firmware.factory.bin"


def get_addr2line_path() -> Path:
    return _toolchain_tool("addr2line")


def get_objdump_path() -> Path:
    return _toolchain_tool("objdump")


def get_readelf_path() -> Path:
    return _toolchain_tool("readelf")


def run_compile(config: ConfigType, verbose: bool) -> int:
    from esphome.arduino8266 import framework
    from esphome.build_gen import arduino8266 as build_gen

    _warn_ignored_platformio_options()
    paths = framework.check_and_install(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
    # Resolved once: the probe is not free and three consumers need it
    ccache = resolve_ccache_path()
    build_gen.write_project(paths, ccache)

    build_dir = get_build_dir()
    env = framework.get_build_env(paths.toolchain, ccache)

    # The DB is a pure function of build.ninja; regenerate when it is older
    compdb = build_dir / "compile_commands.json"
    ninja_file = build_dir / "build.ninja"
    if not compdb.is_file() or compdb.stat().st_mtime < ninja_file.stat().st_mtime:
        _write_compile_commands(paths.ninja, build_dir, env)

    cmd = [str(paths.ninja)]
    if verbose:
        cmd.append("-v")
    if jobs := config[CONF_ESPHOME].get(CONF_COMPILE_PROCESS_LIMIT):
        cmd += ["-j", str(jobs)]
    # Explicit targets: a generator defect that drops them fails loudly
    # instead of a green no-op run leaving stale artifacts in place
    targets = ["firmware.factory.bin", "firmware.ota.bin"]
    cmd += targets

    # cwd, not -C: drops ninja's "Entering directory" banner
    _LOGGER.debug("Running: %s", " ".join(cmd))
    rc = subprocess.run(
        cmd, cwd=build_dir, env=env, check=False, close_fds=False
    ).returncode
    if rc != 0:
        return rc

    # ninja already refused missing targets; existence covers a rule that
    # ran but wrote elsewhere
    build_dir_artifacts = (
        get_elf_path(),
        build_dir / "firmware.bin",
        get_factory_firmware_path(),
        build_dir / "firmware.ota.bin",
    )
    for artifact in build_dir_artifacts:
        if not artifact.is_file():
            _LOGGER.error("Build produced no %s", artifact)
            return 1

    if not _print_size_summary(build_dir, paths):
        # Cause already warned; name the consequence for CI harnesses
        _LOGGER.warning("Firmware size summary unavailable for this build")
    from esphome.build_helpers.idedata import warn_if_idedata_missing

    warn_if_idedata_missing(lambda: get_idedata(ccache))
    return 0


def _write_compile_commands(
    ninja_path: Path, build_dir: Path, env: dict[str, str]
) -> None:
    compdb = build_dir / "compile_commands.json"
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
    write_file(compdb, result.stdout)


def _parse_app_size(build_dir: Path, paths: InstalledPaths) -> int | None:
    """Read the app flash budget (irom0_0_seg length) from the linker script."""
    from esphome.build_gen.arduino8266 import get_flash_ld_path
    from esphome.components.esp8266.build_surgery import segment_length

    # Warnings, not debug: without the app size the Flash summary line is
    # dropped and CI's memory-impact extraction loses its flash metric.
    ld_path = get_flash_ld_path(build_dir, paths)
    try:
        ld_text = ld_path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as err:
        # A corrupt script degrades the same way, never aborts the build
        _LOGGER.warning("Cannot read linker script for the Flash summary: %s", err)
        return None
    if not (app_size := segment_length(ld_text, "irom0_0_seg")):
        _LOGGER.warning("No usable irom0_0_seg in %s; skipping Flash summary", ld_path)
        return None
    return app_size


def _print_size_summary(build_dir: Path, paths: InstalledPaths) -> bool:
    """Print the RAM/Flash lines ``ci_memory_impact_extract.py`` parses;
    False when skipped."""
    from esphome.arduino8266.framework import toolchain_tool
    from esphome.build_helpers.size_summary import print_size_line

    try:
        result = subprocess.run(
            [
                str(toolchain_tool(paths.toolchain, "size")),
                "-A",
                "-d",
                str(build_dir / "firmware.elf"),
            ],
            capture_output=True,
            text=True,
            check=True,
            close_fds=False,
        )
    except (OSError, subprocess.CalledProcessError) as err:
        # The summary is a bonus artifact like idedata; a truncated
        # toolchain extraction must not discard an already-linked build
        _LOGGER.warning("Could not summarize firmware size: %s", err)
        return False
    # -d prints decimal sizes; anything else trips the missing-sections guard
    sections = {
        parts[0]: int(parts[1])
        for line in result.stdout.splitlines()
        if (parts := line.split())[:1] and parts[0].startswith(".") and len(parts) >= 2
        if parts[1].isdigit()
    }
    if missing := set(_RAM_SECTIONS + _FLASH_SECTIONS) - set(sections):
        # A defaulted 0 would print a confidently wrong total for CI's metric
        _LOGGER.warning(
            "Size output is missing section(s) %s; skipping the size summary",
            ", ".join(sorted(missing)),
        )
        return False
    # Resolve the flash budget before printing: a RAM line without its
    # Flash line would skew CI's memory-impact extraction
    app_size = _parse_app_size(build_dir, paths)
    if not app_size:
        return False
    ram = sum(sections[s] for s in _RAM_SECTIONS)
    flash = sum(sections[s] for s in _FLASH_SECTIONS)
    print_size_line("RAM", ram, _MAX_RAM_SIZE)
    print_size_line("Flash", flash, app_size)
    return True


def get_idedata(ccache: str | None = None) -> dict | None:
    """Derive idedata from the build's compile_commands.json (same
    contract as ``espidf.toolchain.get_idedata``)."""
    from esphome.build_helpers.idedata import load_or_build_idedata

    # A disabled ccache resolves to None without spawning anything, so
    # re-resolving here costs nothing when the caller has no answer
    launcher = ccache or resolve_ccache_path()
    return load_or_build_idedata(
        get_build_dir() / "compile_commands.json",
        get_elf_path(),
        # Suffixed so a platformio->arduino->platformio round trip on one
        # config never serves the other toolchain's cache shape
        CORE.relative_internal_path("idedata", f"{CORE.name}.arduino.json"),
        # The compile DB's commands carry the same ccache prefix the ninja
        # rules were generated with
        launcher=str(launcher) if launcher else None,
    )
