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

import functools
import logging
import os
from pathlib import Path
import shutil

from esphome.core import EsphomeError, Version
from esphome.framework_helpers import (
    ccache_defaults_env,
    resolve_ccache_path,
    str_to_lst_of_str,
    tools_cache_path,
)
from esphome.platformio.registry import install_package

_LOGGER = logging.getLogger(__name__)

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


# 3.1.1 rather than 3.1.0: the registry has no package for 3.1.0
MIN_FRAMEWORK_VERSION = Version(3, 1, 1)


def framework_package_version(ver: Version) -> str:
    """Map an Arduino core version (e.g. 3.1.2) to its package version.

    Same encoding as the PlatformIO package registry uses for core 3.x
    releases (3.1.2 -> 3.30102.0). The native toolchain only supports core
    >= MIN_FRAMEWORK_VERSION, so the 1.x/2.x encodings never apply here.
    """
    return f"3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"


def get_framework_path(package_version: str) -> Path:
    return get_arduino8266_tools_path() / "frameworks" / package_version


def get_toolchain_path() -> Path:
    return get_arduino8266_tools_path() / "toolchains" / TOOLCHAIN_VERSION


def _find_ninja() -> Path:
    """Locate the ninja binary: PATH first, else the ninja PyPI wheel.

    The wheel is a requirements.txt dependency, so pip has already
    integrity-checked it; no download logic is needed here.
    """
    if binary := shutil.which("ninja"):
        return Path(binary)
    try:
        import ninja
    except ImportError:
        wheel_binary = None
    else:
        wheel_binary = Path(ninja.BIN_DIR) / (
            "ninja.exe" if os.name == "nt" else "ninja"
        )
    if wheel_binary is None or not wheel_binary.is_file():
        raise EsphomeError(
            "ninja not found on PATH or in the ninja package; reinstall the "
            "esphome Python environment"
        )
    return wheel_binary


def check_and_install(framework_version: Version) -> dict[str, Path]:
    """Ensure framework, toolchain, and ninja are installed; return their paths."""
    if framework_version < MIN_FRAMEWORK_VERSION:
        # Config validation enforces this too; keep the module honest when
        # called directly.
        raise EsphomeError(
            f"The native toolchain requires the Arduino core "
            f">= {MIN_FRAMEWORK_VERSION}, got {framework_version}"
        )
    # Probe the cheap local dependency before ~110 MB of downloads
    ninja_path = _find_ninja()
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
        expect=("bin",),
    )
    return {
        "framework_path": framework_path,
        "toolchain_path": toolchain_path,
        "ninja_path": ninja_path,
    }


def get_build_env(toolchain_path: Path) -> dict[str, str]:
    env = os.environ.copy()
    env["PATH"] = str(toolchain_path / "bin") + os.pathsep + env.get("PATH", "")
    env.update(ccache_env())
    return env


@functools.cache
def ccache_path() -> str | None:
    """The ccache binary to prefix compiles with, or None when disabled."""
    return resolve_ccache_path()


def ccache_env() -> dict[str, str]:
    """Return ccache settings for the build subprocess (not os.environ).

    Mirrors ``espidf.framework._ccache_env``: cache under the machine-global
    tools dir, depend mode (gcc emits depfiles via -MMD), and CCACHE_BASEDIR
    scoped to the build dir so devices share framework cache entries. Values
    the user already set in the environment are respected.
    """
    if ccache_path() is None:
        return {}
    return ccache_defaults_env(get_arduino8266_tools_path() / "ccache")
