"""Native Arduino ESP8266 build driver (the PlatformIO ``run`` equivalent)."""

from __future__ import annotations

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

_RAM_SECTIONS = (".data", ".rodata", ".bss")
_FLASH_SECTIONS = (".irom0.text", ".text", ".text1", ".data", ".rodata")


def get_build_dir() -> Path:
    return CORE.relative_pioenvs_path(CORE.name)


def get_elf_path() -> Path:
    return get_build_dir() / "firmware.elf"


def _toolchain_tool(name: str) -> Path:
    return framework.get_toolchain_path() / "bin" / f"xtensa-lx106-elf-{name}"


def get_addr2line_path() -> Path:
    return _toolchain_tool("addr2line")


def get_objdump_path() -> Path:
    return _toolchain_tool("objdump")


def get_readelf_path() -> Path:
    return _toolchain_tool("readelf")


def run_compile(config: ConfigType, verbose: bool) -> int:
    from esphome.build_gen import arduino8266 as build_gen

    paths = framework.check_and_install(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
    ninja_changed = build_gen.write_project(paths)

    build_dir = get_build_dir()
    env = framework.get_build_env(paths["toolchain_path"])
    cmd = [str(paths["ninja_path"]), "-C", str(build_dir)]
    if verbose:
        cmd.append("-v")
    if jobs := config[CONF_ESPHOME].get(CONF_COMPILE_PROCESS_LIMIT):
        cmd += ["-j", str(jobs)]

    _LOGGER.debug("Running: %s", " ".join(cmd))
    rc = subprocess.run(cmd, env=env, check=False, close_fds=False).returncode
    if rc != 0:
        return rc

    # The compile database is a pure function of build.ninja; skip its
    # regeneration (a ninja spawn plus MBs of text) on unchanged builds.
    if ninja_changed or not (build_dir / "compile_commands.json").is_file():
        _write_compile_commands(paths["ninja_path"], build_dir, env)
    _print_size_summary(build_dir, paths["toolchain_path"])
    get_idedata()
    return 0


def _write_compile_commands(
    ninja_path: Path, build_dir: Path, env: dict[str, str]
) -> None:
    result = subprocess.run(
        [str(ninja_path), "-C", str(build_dir), "-t", "compdb", "cc", "cxx", "asm"],
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
    # write_file_if_changed keeps the mtime stable on no-op builds so the
    # idedata cache in get_idedata() stays valid.
    write_file_if_changed(build_dir / "compile_commands.json", result.stdout)


def _parse_app_size(build_dir: Path) -> int | None:
    """Read the app flash budget (irom0_0_seg length) from the linker script."""
    from esphome.build_gen.arduino8266 import get_flash_ld_path
    from esphome.components.esp8266.build_surgery import segment_length

    # Warnings, not debug: without the app size the Flash summary line is
    # dropped and CI's memory-impact extraction loses its flash metric.
    ld_path = get_flash_ld_path(build_dir)
    try:
        ld_text = ld_path.read_text(encoding="utf-8")
    except OSError as err:
        _LOGGER.warning("Cannot read linker script for the Flash summary: %s", err)
        return None
    app_size = segment_length(ld_text, "irom0_0_seg")
    if app_size is None:
        _LOGGER.warning("irom0_0_seg not found in %s; skipping Flash summary", ld_path)
    return app_size


def _print_size_summary(build_dir: Path, toolchain_path: Path) -> None:
    """Print the PlatformIO-shaped RAM/Flash lines.

    The exact shape (including the bar) is parsed by
    ``script/ci_memory_impact_extract.py``; ``format_bar`` matches it.
    """
    from esphome.espidf.size_summary import format_bar

    size_tool = toolchain_path / "bin" / "xtensa-lx106-elf-size"
    result = subprocess.run(
        [str(size_tool), "-A", "-d", str(get_elf_path())],
        capture_output=True,
        text=True,
        check=False,
        close_fds=False,
    )
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
                # An omitted section would silently skew the reported totals
                _LOGGER.warning("Unparsable size output for section %s", parts[0])
                continue
    ram = sum(sections.get(s, 0) for s in _RAM_SECTIONS)
    flash = sum(sections.get(s, 0) for s in _FLASH_SECTIONS)
    print(f"RAM:   {format_bar(ram, _MAX_RAM_SIZE)}")
    if app_size := _parse_app_size(build_dir):
        print(f"Flash: {format_bar(flash, app_size)}")


def get_idedata() -> dict | None:
    """Derive idedata from the build's compile_commands.json.

    Same contract as ``espidf.toolchain.get_idedata``: the fields IDE
    integrations, clang-tidy, and the memory analyzer expect.
    """
    from esphome.espidf.idedata import load_or_build_idedata

    return load_or_build_idedata(
        get_build_dir() / "compile_commands.json",
        get_elf_path(),
        CORE.relative_internal_path("idedata", f"{CORE.name}.json"),
    )
