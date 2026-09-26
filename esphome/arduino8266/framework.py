"""Download and install the Arduino ESP8266 core, toolchain, and ninja.

Artifacts land in a machine-global cache (shared across projects, like the
ESP-IDF install in ``esphome.espidf.framework``):

    <cache>/arduino8266/frameworks/<tag>/   the Arduino core
    <cache>/arduino8266/toolchains/<tag>/   xtensa-lx106-elf gcc 10.3

Both come from esphome-libs releases pinned below;
``ESPHOME_ARDUINO8266_*_MIRRORS`` overrides the URLs, with ``{VERSION}``
standing for the release tag. ninja comes from PATH or the ninja PyPI wheel.
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import NamedTuple

from esphome.build_helpers.ccache import ccache_env
from esphome.build_helpers.ninja import find_ninja
from esphome.build_helpers.tools_cache import ARDUINO8266_TOOLS_CACHE, tools_cache_path
from esphome.core import EsphomeError, Version
from esphome.framework_helpers import str_to_lst_of_str
from esphome.platformio.registry import (
    Download,
    Resolver,
    get_systype,
    install_packages,
    prefetch_packages,
)

FRAMEWORK_PACKAGE = "arduino-esp8266"
_FRAMEWORK_RELEASES = "https://github.com/esphome-libs/arduino-esp8266/releases/"


class FrameworkRelease(NamedTuple):
    tag: str
    sha256: str
    size: int

    def download(self) -> Download:
        archive = f"{FRAMEWORK_PACKAGE}-{self.tag}.tar.gz"
        url = f"{_FRAMEWORK_RELEASES}download/{self.tag}/{archive}"
        return Download(url, self.sha256, self.size)


# Arduino core version -> its build in esphome-libs/arduino-esp8266
FRAMEWORK_RELEASES: dict[Version, FrameworkRelease] = {
    Version(3, 1, 2): FrameworkRelease(
        "3.1.2-esphome.1",
        "e80751e3123676b967143e39c61f2d8693946db4c7806f2a83dcaaf797ecd582",
        37189311,
    ),
}

TOOLCHAIN_PACKAGE = "toolchain-xtensa-lx106-elf"
# gcc 10.3, the toolchain Arduino core 3.x builds with; the build
# generator's compile flags are tuned to it.
TOOLCHAIN_VERSION = "10.3.0-esphome.2"
_TOOLCHAIN_RELEASES = (
    "https://github.com/esphome-libs/xtensa-lx106-elf-toolchain/releases/"
)
# Registry system tag -> (sha256, size) of that host's archive
TOOLCHAIN_BUILDS: dict[str, tuple[str, int]] = {
    "darwin_arm64": (
        "849cede44d4d5c6ea0f14099783239f559f46327bea314281814f2652b486201",
        60830321,
    ),
    "darwin_x86_64": (
        "ca69904daabf0c5983b372423e5e62f49182a793e992c052e94666852470c897",
        64149487,
    ),
    "linux_aarch64": (
        "60a49a4f082bf246544bd409a9517dbbcab19bb30ac9decbee544b896aaccbd6",
        67573397,
    ),
    "linux_x86_64": (
        "1fba33ca1494ec79f2776e0e37eca93282d30f8bb9992f5f4f9a655d6fff1db4",
        68431336,
    ),
    "windows_amd64": (
        "af9066b0e5bf036f04f2bd9d08b89b81a7f183c57dac0abcaff71dd861cf5f3b",
        67664137,
    ),
}

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


def framework_release(version: Version) -> FrameworkRelease:
    if (release := FRAMEWORK_RELEASES.get(version)) is None:
        raise EsphomeError(
            f"'toolchain: arduino' has no build of Arduino core {version}; "
            f"available: {', '.join(str(v) for v in FRAMEWORK_RELEASES)}. "
            "Use one of those or 'toolchain: platformio'"
        )
    return release


def get_framework_path(tag: str) -> Path:
    return get_arduino8266_tools_path() / "frameworks" / tag


def get_toolchain_path() -> Path:
    return get_arduino8266_tools_path() / "toolchains" / TOOLCHAIN_VERSION


def toolchain_download() -> Download:
    """The toolchain archive for the current host."""
    systype = get_systype()
    if (build := TOOLCHAIN_BUILDS.get(systype)) is None:
        raise EsphomeError(
            f"There is no ESP8266 toolchain for this system ({systype}); "
            f"supported systems are {', '.join(sorted(TOOLCHAIN_BUILDS))}. "
            "Either set 'toolchain: platformio' under 'esp8266:', or point "
            "ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS at a toolchain archive"
        )
    sha256, size = build
    archive = f"{TOOLCHAIN_PACKAGE}-{TOOLCHAIN_VERSION}-{systype}.tar.gz"
    url = f"{_TOOLCHAIN_RELEASES}download/{TOOLCHAIN_VERSION}/{archive}"
    return Download(url, sha256, size)


class InstalledPaths(NamedTuple):
    """Locations of the installed framework, toolchain, and ninja binary."""

    framework: Path
    toolchain: Path
    ninja: Path


def check_and_install(framework_version: Version) -> InstalledPaths:
    """Ensure framework, toolchain, and ninja are installed; return their paths."""
    release = framework_release(framework_version)
    # Probe the cheap local dependency before ~110 MB of downloads
    ninja_path = find_ninja()
    framework_path = get_framework_path(release.tag)
    downloads_dir = get_arduino8266_tools_path() / "downloads"
    toolchain_path = get_toolchain_path()
    # One spec per package: the prefetch and the installs must agree
    specs = (
        (
            FRAMEWORK_PACKAGE,
            release.tag,
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
    # Resolved only when a download is needed, so an installed toolchain
    # keeps working on a host without a build; a mirror override wins
    resolvers: dict[str, Resolver] = {}
    if not ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS:
        resolvers[FRAMEWORK_PACKAGE] = release.download
    if not ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS:
        resolvers[TOOLCHAIN_PACKAGE] = toolchain_download
    # Fetch both archives at once; the installs verify and extract them.
    # One spec list for both, so the two phases cannot drift.
    prefetch_packages(specs, downloads_dir, resolvers)
    install_packages(specs, downloads_dir, resolvers)
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
    env.update(ccache_env(ccache, ARDUINO8266_TOOLS_CACHE))
    return env
