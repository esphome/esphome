"""Download and install the Arduino ESP8266 core, toolchain, and ninja.

Artifacts land in a machine-global cache (shared across projects, like the
ESP-IDF install in ``esphome.espidf.framework``):

    <cache>/arduino8266/frameworks/<version>/   framework-arduinoespressif8266
    <cache>/arduino8266/toolchains/<version>/   toolchain-xtensa (gcc 10.3)

Packages come from the PlatformIO registry (identical bits to the PlatformIO
backend); ``ESPHOME_ARDUINO8266_*_MIRRORS`` overrides the URLs. ninja comes
from PATH or the ninja PyPI wheel.
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import NamedTuple

from esphome.build_helpers.ccache import ccache_defaults_env
from esphome.build_helpers.ninja import find_ninja
from esphome.build_helpers.tools_cache import ARDUINO8266_TOOLS_CACHE, tools_cache_path
from esphome.core import EsphomeError, Version
from esphome.framework_helpers import str_to_lst_of_str
from esphome.platformio.registry import install_package, prefetch_packages

FRAMEWORK_PACKAGE = "framework-arduinoespressif8266"
TOOLCHAIN_PACKAGE = "toolchain-xtensa"
# gcc 10.3, the toolchain Arduino core 3.x builds with; the build
# generator's compile flags are tuned to it.
TOOLCHAIN_VERSION = "2.100300.220621"

ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS = str_to_lst_of_str(
    os.environ.get("ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS", "")
)
ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS = str_to_lst_of_str(
    os.environ.get("ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS", "")
)


def get_arduino8266_tools_path() -> Path:
    # Machine-global so all projects share one install; see
    # espidf.framework.get_idf_tools_path for the location rationale.
    return tools_cache_path(*ARDUINO8266_TOOLS_CACHE)


# 3.1.1 rather than 3.1.0: the registry has no package for 3.1.0, and the
# encoder below cannot name 3.0.0/3.0.1 either (see its docstring)
MIN_FRAMEWORK_VERSION = Version(3, 1, 1)


def framework_package_version(ver: Version) -> str:
    """Map an Arduino core version to its registry package version (3.1.2 ->
    3.30102.0; the leading 3 is the package major).

    Exact registry names only for cores > 2.6.2 and >= 3.0.2; callers floor
    at MIN_FRAMEWORK_VERSION.
    """
    if ver.major > 3:
        raise EsphomeError(
            f"Arduino core {ver} is not supported yet; "
            "the newest known core series is 3.x"
        )
    if ver <= Version(2, 6, 2):
        # Cores <= 2.6.2 use the older 1.x/2.x package-major encodings (same
        # boundary as _format_framework_arduino_version's era guard)
        raise EsphomeError(
            f"Arduino core {ver} uses an older package encoding than this "
            "helper implements (newer than 2.6.2)"
        )
    return f"3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"


def get_framework_path(package_version: str) -> Path:
    return get_arduino8266_tools_path() / "frameworks" / package_version


def get_toolchain_path() -> Path:
    return get_arduino8266_tools_path() / "toolchains" / TOOLCHAIN_VERSION


class InstalledPaths(NamedTuple):
    """Locations of the installed framework, toolchain, and ninja binary."""

    framework: Path
    toolchain: Path
    ninja: Path


def check_and_install(framework_version: Version) -> InstalledPaths:
    """Ensure framework, toolchain, and ninja are installed; return their paths."""
    if framework_version < MIN_FRAMEWORK_VERSION:
        # Config validation enforces this too; keep the module honest when
        # called directly.
        raise EsphomeError(
            f"The native toolchain requires the Arduino core "
            f">= {MIN_FRAMEWORK_VERSION}, got {framework_version}"
        )
    # Probe the cheap local dependency before ~110 MB of downloads
    ninja_path = find_ninja()
    package_version = framework_package_version(framework_version)
    framework_path = get_framework_path(package_version)
    downloads_dir = get_arduino8266_tools_path() / "downloads"
    toolchain_path = get_toolchain_path()
    # One spec per package: the prefetch and the installs must agree
    specs = (
        (
            FRAMEWORK_PACKAGE,
            package_version,
            framework_path,
            ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS,
            ("cores/esp8266", "tools/sdk", "libraries"),
        ),
        (
            TOOLCHAIN_PACKAGE,
            TOOLCHAIN_VERSION,
            toolchain_path,
            ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS,
            # xtensa-lx106-elf pins the target: every gcc package has a bin/
            ("bin", "xtensa-lx106-elf"),
        ),
    )
    # Fetch both archives at once; the installs below verify and extract
    prefetch_packages([spec[:4] for spec in specs], downloads_dir)
    for name, version, dest, mirrors, expect in specs:
        install_package(name, version, dest, mirrors, downloads_dir, expect=expect)
    return InstalledPaths(
        framework=framework_path, toolchain=toolchain_path, ninja=ninja_path
    )


def toolchain_tool(toolchain_path: Path, name: str) -> Path:
    """Path to one toolchain tool (gcc, g++, ar, size, addr2line, ...).

    The single owner of the ``bin/xtensa-lx106-elf-<name>`` layout and the
    Windows suffix, so a toolchain package bump touches one spot.
    """
    suffix = ".exe" if os.name == "nt" else ""
    return toolchain_path / "bin" / f"xtensa-lx106-elf-{name}{suffix}"


def get_build_env(toolchain_path: Path, ccache: str | None) -> dict[str, str]:
    env = os.environ.copy()
    # Drop empty entries: a trailing separator from an absent PATH would
    # make the shell search the current directory for tools
    parts = [
        str(toolchain_path / "bin"),
        *filter(None, env.get("PATH", "").split(os.pathsep)),
    ]
    env["PATH"] = os.pathsep.join(parts)
    env.update(ccache_env(ccache))
    return env


def ccache_env(ccache: str | None) -> dict[str, str]:
    """Return ccache settings for the build subprocess (not os.environ).

    ``ccache`` is the pre-resolved binary (resolve_ccache_path), or None
    when disabled. Values the user already set in the environment are
    respected.
    """
    if ccache is None:
        return {}
    return ccache_defaults_env(get_arduino8266_tools_path() / "ccache")
