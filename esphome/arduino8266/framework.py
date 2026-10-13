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
    PackageSpec,
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
        "3.1.2-esphome.3",
        "3af533a85163804e164500a5b254dc23b922080e25ecb7dc37b3dd8ac84651db",
        37185650,
    ),
}

TOOLCHAIN_PACKAGE = "toolchain-xtensa-lx106-elf"
# gcc 10.3, the toolchain Arduino core 3.x builds with; the build
# generator's compile flags are tuned to it.
TOOLCHAIN_VERSION = "10.3.0-esphome.3"
_TOOLCHAIN_RELEASES = (
    "https://github.com/esphome-libs/xtensa-lx106-elf-toolchain/releases/"
)
# Registry system tag -> (sha256, size) of that host's archive
TOOLCHAIN_BUILDS: dict[str, tuple[str, int]] = {
    "darwin_arm64": (
        "ad0ea929238d9b527c43e0d20257acc2a5680bdb0d2b1cf8f2c99169786217e9",
        60747260,
    ),
    "darwin_x86_64": (
        "997090cac80c8eb59604ec52b73fb3568e5f2d9b2971f83c3f2a5c545c9c949c",
        64065752,
    ),
    "linux_aarch64": (
        "403f14d0d5755682fcd6eaea1e4c457a1d8cad30690e52fb0c804d8db266b908",
        67487067,
    ),
    "linux_x86_64": (
        "64f21fa3ba736b363dc8c234dbbb39e005e574e33de08a78e9a00c55a64ae706",
        68343668,
    ),
    "windows_amd64": (
        "1e62abf8df041ce5158f5770dffef955f2f981ce7f7adee88fcd5a8197705aa6",
        67536405,
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
        PackageSpec(
            FRAMEWORK_PACKAGE,
            release.tag,
            framework_path,
            ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS,
            ("cores/esp8266", "tools/sdk", "libraries"),
        ),
        PackageSpec(
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
