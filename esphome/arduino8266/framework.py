"""Download and install the Arduino ESP8266 core, toolchain, and ninja.

Artifacts land in a machine-global cache (shared across projects, like the
ESP-IDF install in ``esphome.espidf.framework``):

    <cache>/arduino8266/frameworks/<version>/   framework-arduinoespressif8266
    <cache>/arduino8266/toolchains/<version>/   toolchain-xtensa (gcc 10.3)
    <cache>/arduino8266/tools/ninja/            ninja (only if not on PATH)

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
import platform
import shutil
import stat
import tempfile

import platformdirs

import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import (
    archive_extract_all,
    download_from_mirrors,
    rmdir,
    str_to_lst_of_str,
)
from esphome.helpers import get_bool_env, get_str_env

_LOGGER = logging.getLogger(__name__)

FRAMEWORK_PACKAGE = "framework-arduinoespressif8266"
TOOLCHAIN_PACKAGE = "toolchain-xtensa"
# gcc 10.3, the toolchain Arduino core 3.x builds with. The compile flags in
# the build generator are tuned to it; treat version changes as a full
# reinstall (the install dir is keyed on the version).
TOOLCHAIN_VERSION = "2.100300.220621"

NINJA_VERSION = "1.12.1"

_REGISTRY_URL = (
    "https://api.registry.platformio.org/v3/packages/platformio/tool/{package}"
)

ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS = str_to_lst_of_str(
    os.environ.get("ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS", "")
)
ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS = str_to_lst_of_str(
    os.environ.get("ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS", "")
)
ESPHOME_ARDUINO8266_NINJA_MIRRORS = str_to_lst_of_str(
    os.environ.get(
        "ESPHOME_ARDUINO8266_NINJA_MIRRORS",
        "https://github.com/ninja-build/ninja/releases/download/v{VERSION}/{ARCHIVE}",
    )
)


def get_arduino8266_tools_path() -> Path:
    # Treat an empty/whitespace prefix as unset: Path("") resolves to the CWD,
    # which clean-all would then delete.
    if prefix := get_str_env("ESPHOME_ARDUINO8266_PREFIX", "").strip():
        path = Path(prefix).expanduser()
    else:
        # Machine-global so all projects share one install; see
        # espidf.framework.get_idf_tools_path for the location rationale.
        path = (
            Path(platformdirs.user_cache_dir("esphome", appauthor=False))
            / "arduino8266"
        )
    return path.resolve()


def framework_package_version(ver: cv.Version) -> str:
    """Map an Arduino core version (e.g. 3.1.2) to its package version.

    Same encoding as the PlatformIO package registry uses for core 3.x
    releases (3.1.2 -> 3.30102.0). The native toolchain only supports core
    >= 3.1.0, so the 1.x/2.x encodings never apply here.
    """
    return f"3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"


def get_framework_path(package_version: str) -> Path:
    return get_arduino8266_tools_path() / "frameworks" / package_version


def get_toolchain_path() -> Path:
    return get_arduino8266_tools_path() / "toolchains" / TOOLCHAIN_VERSION


def _pio_system() -> str:
    """The PlatformIO registry system tag for the current host.

    Hand-rolled instead of ``platformio.util.get_systype()`` so this backend
    never imports the PlatformIO package. The windows-arm64 and darwin-arm64
    mappings are deliberate: the toolchain packages ship x86_64 binaries for
    those hosts (Rosetta / x86 emulation).
    """
    sysname = platform.system().lower()
    machine = platform.machine().lower()
    if sysname == "darwin":
        return "darwin_arm64" if machine == "arm64" else "darwin_x86_64"
    if sysname == "windows":
        return "windows_amd64" if machine in ("amd64", "arm64") else "windows_x86"
    if machine in ("arm64", "aarch64"):
        return "linux_aarch64"
    if machine in ("i686", "i386", "x86"):
        return "linux_i686"
    if machine.startswith("arm"):
        return f"linux_{machine}"
    return "linux_x86_64"


def _registry_download_url(package: str, version: str) -> str:
    """Resolve a package's download URL for this host via the PIO registry."""
    import requests

    url = _REGISTRY_URL.format(package=package)
    resp = requests.get(url, timeout=30)
    resp.raise_for_status()
    data = resp.json()
    system = _pio_system()
    for ver in data.get("versions", []):
        if ver.get("name") != version:
            continue
        for file in ver.get("files", []):
            systems = file.get("system") or "*"
            if systems == "*" or system in systems:
                return file["download_url"]
        raise EsphomeError(f"No {package} {version} build for this platform ({system})")
    raise EsphomeError(f"{package} {version} not found in the package registry")


def _install_package(
    name: str,
    version: str,
    dest: Path,
    mirrors: list[str],
) -> None:
    """Download and extract one package if not already installed."""
    marker = dest / ".esphome_extracted"
    if marker.is_file():
        return
    rmdir(dest, msg=f"Clean up incomplete {name} install")
    with tempfile.NamedTemporaryFile() as tmp:
        _LOGGER.info("Downloading %s %s ...", name, version)
        if mirrors:
            download_from_mirrors(
                mirrors, {"VERSION": version, "SYSTEM": _pio_system()}, tmp.file
            )
        else:
            download_from_mirrors([_registry_download_url(name, version)], {}, tmp.file)
        _LOGGER.info("Extracting %s ...", name)
        archive_extract_all(tmp.file, dest, progress_header="Extracting")
    marker.touch()


def _ninja_archive_name() -> str:
    sysname = platform.system().lower()
    machine = platform.machine().lower()
    if sysname == "darwin":
        return "ninja-mac.zip"
    if sysname == "windows":
        return "ninja-winarm64.zip" if machine == "arm64" else "ninja-win.zip"
    if machine in ("arm64", "aarch64"):
        return "ninja-linux-aarch64.zip"
    return "ninja-linux.zip"


def _check_ninja_install() -> Path:
    """Return a usable ninja binary, downloading one if none is on PATH."""
    if ninja := shutil.which("ninja"):
        return Path(ninja)
    ninja_dir = get_arduino8266_tools_path() / "tools" / "ninja" / NINJA_VERSION
    binary = ninja_dir / ("ninja.exe" if os.name == "nt" else "ninja")
    if binary.is_file():
        return binary
    rmdir(ninja_dir, msg="Clean up incomplete ninja install")
    with tempfile.NamedTemporaryFile() as tmp:
        _LOGGER.info("Downloading ninja %s ...", NINJA_VERSION)
        download_from_mirrors(
            ESPHOME_ARDUINO8266_NINJA_MIRRORS,
            {"VERSION": NINJA_VERSION, "ARCHIVE": _ninja_archive_name()},
            tmp.file,
        )
        archive_extract_all(tmp.file, ninja_dir)
    if not binary.is_file():
        raise EsphomeError(f"ninja binary missing after extraction in {ninja_dir}")
    binary.chmod(binary.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    return binary


def check_and_install(framework_version: cv.Version) -> dict[str, Path]:
    """Ensure framework, toolchain, and ninja are installed; return their paths."""
    package_version = framework_package_version(framework_version)
    framework_path = get_framework_path(package_version)
    _install_package(
        FRAMEWORK_PACKAGE,
        package_version,
        framework_path,
        ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS,
    )
    toolchain_path = get_toolchain_path()
    _install_package(
        TOOLCHAIN_PACKAGE,
        TOOLCHAIN_VERSION,
        toolchain_path,
        ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS,
    )
    return {
        "framework_path": framework_path,
        "toolchain_path": toolchain_path,
        "ninja_path": _check_ninja_install(),
    }


def get_build_env(toolchain_path: Path) -> dict[str, str]:
    env = os.environ.copy()
    env["PATH"] = str(toolchain_path / "bin") + os.pathsep + env.get("PATH", "")
    env.update(ccache_env())
    return env


@functools.cache
def ccache_path() -> str | None:
    """The ccache binary to prefix compiles with, or None when disabled.

    Same opt-out convention as the PlatformIO path: on by default when the
    binary is on PATH, ``ESPHOME_CCACHE_ENABLE=0`` disables it.
    """
    from esphome.platformio.toolchain import _ccache_runs, _strip_win_long_path_prefix

    if "ESPHOME_CCACHE_ENABLE" in os.environ and not get_bool_env(
        "ESPHOME_CCACHE_ENABLE"
    ):
        return None
    ccache = shutil.which("ccache")
    if ccache is None:
        return None
    ccache = _strip_win_long_path_prefix(ccache)
    return ccache if _ccache_runs(ccache) else None


def ccache_env() -> dict[str, str]:
    """Return ccache settings for the build subprocess (not os.environ).

    Mirrors ``espidf.framework._ccache_env``: cache under the machine-global
    tools dir, depend mode (gcc emits depfiles via -MMD), and CCACHE_BASEDIR
    scoped to the build dir so devices share framework cache entries. Values
    the user already set in the environment are respected.
    """
    if ccache_path() is None:
        return {}
    defaults = {
        "CCACHE_DIR": str(get_arduino8266_tools_path() / "ccache"),
        "CCACHE_NOHASHDIR": "true",
        "CCACHE_DEPEND": "1",
        "CCACHE_BASEDIR": str(Path(CORE.build_path).resolve()),
    }
    return {k: v for k, v in defaults.items() if k not in os.environ}
