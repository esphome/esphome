"""Download and install the Arduino ESP8266 core, toolchain, and ninja.

Artifacts land in a machine-global cache (shared across projects, like the
ESP-IDF install in ``esphome.espidf.framework``):

    <cache>/arduino8266/frameworks/<version>/   framework-arduinoespressif8266
    <cache>/arduino8266/toolchains/<version>/   toolchain-xtensa (gcc 10.3)

ninja itself comes from PATH or the ninja PyPI wheel (a requirements.txt
dependency), so only the two packages above are downloaded here.

Sources default to the PlatformIO registry (the exact packages the PlatformIO
toolchain has always used, so the bits are identical); the
``ESPHOME_ARDUINO8266_*_MIRRORS`` environment variables override the URLs with
``{VERSION}`` / ``{SYSTEM}`` substitution.
"""

from __future__ import annotations

from collections.abc import Collection
import functools
import logging
import os
from pathlib import Path
import platform
import shutil
import time

from esphome.core import EsphomeError, Version
from esphome.framework_helpers import (
    archive_extract_all,
    ccache_defaults_env,
    download_from_mirrors,
    download_with_resume,
    resolve_ccache_path,
    rmdir,
    str_to_lst_of_str,
    tools_cache_path,
)

_LOGGER = logging.getLogger(__name__)

FRAMEWORK_PACKAGE = "framework-arduinoespressif8266"
TOOLCHAIN_PACKAGE = "toolchain-xtensa"
# gcc 10.3, the toolchain Arduino core 3.x builds with. The compile flags in
# the build generator are tuned to it; treat version changes as a full
# reinstall (the install dir is keyed on the version).
TOOLCHAIN_VERSION = "2.100300.220621"

_REGISTRY_URL = (
    "https://api.registry.platformio.org/v3/packages/platformio/tool/{package}"
)

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


def _downloads_path() -> Path:
    path = get_arduino8266_tools_path() / "downloads"
    path.mkdir(parents=True, exist_ok=True)
    return path


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
        if machine == "arm64":
            return "darwin_arm64"
        if machine == "x86_64":
            return "darwin_x86_64"
    if sysname == "windows":
        if machine in ("amd64", "arm64"):
            return "windows_amd64"
        if machine in ("x86", "i686", "i386"):
            return "windows_x86"
    if sysname == "linux":
        if machine in ("arm64", "aarch64"):
            return "linux_aarch64"
        if machine in ("i686", "i386", "x86"):
            return "linux_i686"
        if machine.startswith("arm"):
            return f"linux_{machine}"
        if machine in ("x86_64", "amd64"):
            return "linux_x86_64"
    # Fail here, near the cause, rather than installing a toolchain whose
    # binaries cannot execute on this host.
    raise EsphomeError(
        f"No {sysname}/{machine} build of the ESP8266 toolchain exists; "
        "use 'toolchain: platformio'"
    )


def _registry_download(package: str, version: str) -> tuple[str, str, int | None]:
    """Resolve a package's download URL, sha256, and size via the PIO registry."""
    import requests

    url = _REGISTRY_URL.format(package=package)
    last_err: Exception | None = None
    for attempt in range(3):
        try:
            resp = requests.get(url, timeout=30)
            resp.raise_for_status()
            data = resp.json()
            break
        except requests.RequestException as err:
            last_err = err
            # Back off so the retries are not one burst against a hiccup
            time.sleep(2**attempt)
    else:
        # A clean, retried error like the other download paths in the tree
        raise EsphomeError(
            f"Could not query the package registry for {package}: {last_err}"
        ) from last_err
    system = _pio_system()
    for ver in data.get("versions", []):
        if ver.get("name") != version:
            continue
        for file in ver.get("files", []):
            # A bare string would make ``in`` a substring test
            systems = file.get("system") or "*"
            if isinstance(systems, str):
                systems = [systems]
            if "*" in systems or system in systems:
                sha256 = (file.get("checksum") or {}).get("sha256")
                if not sha256:
                    # Never extract an unverified archive; the registry
                    # publishes a checksum for every package file.
                    raise EsphomeError(
                        f"The package registry returned no sha256 for "
                        f"{package} {version}; refusing the unverified download"
                    )
                return (file["download_url"], sha256, file.get("size"))
        raise EsphomeError(f"No {package} {version} build for this platform ({system})")
    raise EsphomeError(f"{package} {version} not found in the package registry")


def _install_package(
    name: str,
    version: str,
    dest: Path,
    mirrors: list[str],
    expect: Collection[str] = (),
) -> None:
    """Download, verify, and extract one package if not already installed.

    The registry path is integrity-checked against the sha256 the registry
    publishes; a mirror override is trusted as configured.
    """
    marker = dest / ".esphome_extracted"
    if marker.is_file():
        return
    from filelock import FileLock

    # The cache is machine-global; serialize concurrent cold builds so one
    # process cannot wipe the directory another is extracting into (same
    # filelock pattern as platformio/toolchain.py and git.py).
    dest.parent.mkdir(parents=True, exist_ok=True)
    # fallback_to_soft would silently degrade to an existence lock on a
    # flock-less filesystem; a hard-killed run would then hang every later
    # build forever (same hazard git.py documents).
    with FileLock(f"{dest}.lock", fallback_to_soft=False):
        if marker.is_file():
            # Another process finished the install while we waited
            return
        rmdir(dest, msg=f"Clean up incomplete {name} install")
        # A persistent download location (not a temp dir) so an interrupted
        # download resumes across esphome runs via download_with_resume's
        # .part file, mirroring the espidf dist/ convention.
        archive = _downloads_path() / f"{name}-{version}"
        _LOGGER.info("Downloading %s %s ...", name, version)
        if mirrors:
            _LOGGER.warning(
                "Downloading %s from a mirror override; checksum verification "
                "is skipped for mirrors",
                name,
            )
            download_from_mirrors(
                mirrors, {"VERSION": version, "SYSTEM": _pio_system()}, archive
            )
        else:
            url, sha256, size = _registry_download(name, version)
            download_with_resume(url, archive, sha256=sha256, size=size)
        _LOGGER.info("Extracting %s ...", name)
        archive_extract_all(archive, dest, progress_header="Extracting")
        # Validate the layout before recording success, so an unexpected
        # package is never cached as a working install.
        for rel in expect:
            if not (dest / rel).is_dir():
                raise EsphomeError(
                    f"{name} {version} extracted without the expected {rel} "
                    "directory; run 'esphome clean-all' and retry"
                )
        marker.touch()
        archive.unlink(missing_ok=True)


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
    _install_package(
        FRAMEWORK_PACKAGE,
        package_version,
        framework_path,
        ESPHOME_ARDUINO8266_FRAMEWORK_MIRRORS,
        expect=("cores/esp8266", "tools/sdk", "libraries"),
    )
    toolchain_path = get_toolchain_path()
    _install_package(
        TOOLCHAIN_PACKAGE,
        TOOLCHAIN_VERSION,
        toolchain_path,
        ESPHOME_ARDUINO8266_TOOLCHAIN_MIRRORS,
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
