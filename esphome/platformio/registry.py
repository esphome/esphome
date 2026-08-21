"""Install packages from the PlatformIO registry without PlatformIO.

Native toolchains install the exact registry packages the PlatformIO backend
uses, so the bits are identical, but resolve and verify them with esphome's
own download machinery instead of importing the platformio package.
"""

from __future__ import annotations

from collections.abc import Collection
import io
import json
import logging
import os
from pathlib import Path
import platform

from esphome.core import EsphomeError
from esphome.framework_helpers import (
    archive_extract_all,
    download_from_mirrors,
    download_with_resume,
    rmdir,
)

_LOGGER = logging.getLogger(__name__)

_REGISTRY_URL = (
    "https://api.registry.platformio.org/v3/packages/platformio/tool/{package}"
)


def get_systype() -> str:
    """The registry system tag for the current host.

    A transliteration of ``platformio.util.get_systype()``, honoring the same
    ``PLATFORMIO_SYSTEM_TYPE`` override, so this module never imports the
    platformio package. One deviation: windows-arm64 maps straight to
    ``windows_amd64``: the registry ships no arm64 toolchains and those hosts
    run x86 binaries via emulation, which upstream leaves to the override.
    """
    if systype := os.environ.get("PLATFORMIO_SYSTEM_TYPE"):
        return systype
    system = platform.system().lower()
    arch = platform.machine().lower()
    if system == "windows":
        if not arch:  # same fallback as upstream (platformio issue #4353)
            arch = "x86_" + platform.architecture()[0]
        if "x86" in arch:
            arch = "amd64" if "64" in arch else "x86"
        elif arch == "arm64":
            arch = "amd64"
    if arch == "aarch64" and platform.architecture()[0] == "32bit":
        # 64-bit kernel with a 32-bit userland (e.g. 32-bit Raspberry Pi OS)
        arch = "armv7l"
    return f"{system}_{arch}" if arch else system


def registry_download(package: str, version: str) -> tuple[str, str, int | None]:
    """Resolve a package's download URL, sha256, and size via the registry.

    The metadata fetch goes through ``download_from_mirrors`` so it shares
    the retry, backoff, and error reporting of every other download here.
    """
    buf = io.BytesIO()
    download_from_mirrors([_REGISTRY_URL], {"package": package}, buf)
    try:
        data = json.loads(buf.getvalue())
    except ValueError as err:
        raise EsphomeError(
            f"The package registry returned invalid JSON for {package}: {err}"
        ) from err
    systype = get_systype()
    versions = data.get("versions")
    if not isinstance(versions, list):
        # A schema change or an error/captive-portal payload must not be
        # reported as "version not found"
        raise EsphomeError(
            f"Unexpected package registry response for {package}: {str(data)[:200]}"
        )
    for ver in versions:
        if ver.get("name") != version:
            continue
        for file in ver.get("files", []):
            # Only a MISSING key means "any system"; an explicitly empty
            # list must not match (a wrong-architecture download would be
            # cached as a good install). A bare string would make ``in`` a
            # substring test.
            systems = file.get("system")
            if systems is None:
                systems = ["*"]
            elif isinstance(systems, str):
                systems = [systems]
            if "*" in systems or systype in systems:
                sha256 = (file.get("checksum") or {}).get("sha256")
                if not sha256:
                    # Never extract an unverified archive; the registry
                    # publishes a checksum for every package file.
                    raise EsphomeError(
                        f"The package registry returned no sha256 for "
                        f"{package} {version}; refusing the unverified download"
                    )
                return (file["download_url"], sha256, file.get("size"))
        raise EsphomeError(
            f"No {package} {version} build for this platform ({systype})"
        )
    raise EsphomeError(f"{package} {version} not found in the package registry")


def install_package(
    name: str,
    version: str,
    dest: Path,
    mirrors: list[str],
    downloads_dir: Path,
    expect: Collection[str],
) -> None:
    """Download, verify, and extract one package if not already installed.

    The registry path is integrity-checked against the sha256 the registry
    publishes; a mirror override (URL templates with ``{VERSION}``/``{SYSTEM}``
    substitution) is trusted as configured. ``downloads_dir`` holds the
    archive between runs so an interrupted download resumes.
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
        downloads_dir.mkdir(parents=True, exist_ok=True)
        archive = downloads_dir / f"{name}-{version}"
        _LOGGER.info("Downloading %s %s ...", name, version)
        if mirrors:
            _LOGGER.warning(
                "Downloading %s from a mirror override; checksum verification "
                "is skipped for mirrors",
                name,
            )
            download_from_mirrors(
                mirrors, {"VERSION": version, "SYSTEM": get_systype()}, archive
            )
        else:
            url, sha256, size = registry_download(name, version)
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
