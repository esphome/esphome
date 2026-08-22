"""Download and install the Arduino ESP8266 core, toolchain, and ninja.

Artifacts land in a machine-global cache (shared across projects, like the
ESP-IDF install in ``esphome.espidf.framework``):

    <cache>/arduino8266/frameworks/<version>/   framework-arduinoespressif8266
    <cache>/arduino8266/toolchains/<version>/   toolchain-xtensa (gcc 10.3)

ninja itself comes from PATH or the ninja PyPI wheel (a requirements.txt
dependency), so only the two packages above are downloaded, via the shared
PlatformIO-registry installer in ``esphome.platformio.registry``.

Sources default to the PlatformIO registry (the exact packages the PlatformIO
toolchain has always used, so the bits are identical); the
``ESPHOME_ARDUINO8266_*_MIRRORS`` environment variables override the URLs with
``{VERSION}`` / ``{SYSTEM}`` substitution.
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any, NamedTuple

from esphome.build_helpers.ccache import ccache_defaults_env, resolve_ccache_path
from esphome.build_helpers.ninja import find_ninja
from esphome.build_helpers.tools_cache import tools_cache_path
from esphome.core import EsphomeError, Version
from esphome.framework_helpers import str_to_lst_of_str
from esphome.platformio.registry import install_package

FRAMEWORK_PACKAGE = "framework-arduinoespressif8266"
TOOLCHAIN_PACKAGE = "toolchain-xtensa"
# gcc 10.3, the toolchain Arduino core 3.x builds with. The compile flags in
# the build generator are tuned to it; treat version changes as a full
# reinstall (the install dir is keyed on the version).
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
    return tools_cache_path("ESPHOME_ARDUINO8266_PREFIX", "arduino8266")


# 3.1.1 rather than 3.1.0: the registry has no package for 3.1.0, and the
# encoder below cannot name 3.0.0/3.0.1 either (see its docstring)
MIN_FRAMEWORK_VERSION = Version(3, 1, 1)


def framework_package_version(ver: Version) -> str:
    """Map an Arduino core version (e.g. 3.1.2) to its package version.

    The PlatformIO registry's encoding for cores newer than 2.6.2 (3.1.2 ->
    3.30102.0, and 2.7.4 -> 3.20704.0: the leading 3 is the package major,
    not the core major). Exact registry names only from 3.0.2 up: 2.6.3,
    3.0.0 and 3.0.1 ship as 3.20603.200130 / 3.30000.210519 /
    3.30001.210627, which this formula cannot produce. Safe for the
    PlatformIO caller (a ~ range) and for check_and_install (floored at
    MIN_FRAMEWORK_VERSION); an exact lookup below that floor must not use
    this helper. A future core 4.x needs its own encoding and toolchain pin
    rather than a registry lookup for a package that cannot exist.
    """
    if ver.major > 3:
        # Backend-neutral: this also fires on the PlatformIO validation path
        # (via _format_framework_arduino_version), where switching toolchains
        # would not help
        raise EsphomeError(
            f"Arduino core {ver} is not supported yet; "
            "the newest known core series is 3.x"
        )
    if ver <= Version(2, 6, 2):
        # Same boundary as _format_framework_arduino_version's era guard (a
        # 2.6.2 pre-release sorts above 2.6.2 and belongs to this encoding).
        # Older cores use the 1.x/2.x package-major encodings; never encode
        # them wrongly for a caller that skipped that guard
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
        # Config validation will enforce this once the native backend is
        # wired in; keep the module honest when called directly.
        raise EsphomeError(
            f"The native toolchain requires the Arduino core "
            f">= {MIN_FRAMEWORK_VERSION}, got {framework_version}"
        )
    # Probe the cheap local dependency before ~110 MB of downloads
    ninja_path = find_ninja()
    package_version = framework_package_version(framework_version)
    framework_path = get_framework_path(package_version)
    downloads_dir = get_arduino8266_tools_path() / "downloads"
    install_package(
        FRAMEWORK_PACKAGE,
        package_version,
        framework_path,
        ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS,
        downloads_dir,
        expect=("cores/esp8266", "tools/sdk", "libraries"),
    )
    toolchain_path = get_toolchain_path()
    install_package(
        TOOLCHAIN_PACKAGE,
        TOOLCHAIN_VERSION,
        toolchain_path,
        ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS,
        downloads_dir,
        # xtensa-lx106-elf pins the target: every gcc package has a bin/
        expect=("bin", "xtensa-lx106-elf"),
    )
    return InstalledPaths(
        framework=framework_path, toolchain=toolchain_path, ninja=ninja_path
    )


# Sentinel: "resolve for me" (None is a real value meaning disabled).
# run_compile resolves once and threads the result so one build never pays
# the PATH scan and runnability probe three times.
CCACHE_UNRESOLVED: Any = object()


def toolchain_tool(toolchain_path: Path, name: str) -> Path:
    """Path to one toolchain tool (gcc, g++, ar, size, addr2line, ...).

    The single owner of the ``bin/xtensa-lx106-elf-<name>`` layout and the
    Windows suffix, so a toolchain package bump touches one spot.
    """
    suffix = ".exe" if os.name == "nt" else ""
    return toolchain_path / "bin" / f"xtensa-lx106-elf-{name}{suffix}"


def get_build_env(
    toolchain_path: Path, ccache: str | None = CCACHE_UNRESOLVED
) -> dict[str, str]:
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


def ccache_path() -> str | None:
    """The ccache binary to prefix compiles with, or None when disabled.

    Deliberately uncached (matching espidf): the decision reads
    ESPHOME_CCACHE_ENABLE and PATH, which can change between builds in a
    long-lived host process.
    """
    return resolve_ccache_path()


def ccache_env(ccache: str | None = CCACHE_UNRESOLVED) -> dict[str, str]:
    """Return ccache settings for the build subprocess (not os.environ).

    Mirrors ``espidf.framework._ccache_env``: cache under the machine-global
    tools dir, depend mode (gcc emits depfiles via -MMD), and CCACHE_BASEDIR
    scoped to the build dir so devices share framework cache entries. Values
    the user already set in the environment are respected.
    """
    if ccache is CCACHE_UNRESOLVED:
        ccache = ccache_path()
    if ccache is None:
        return {}
    return ccache_defaults_env(get_arduino8266_tools_path() / "ccache")
