"""Install packages from the PlatformIO registry without importing the
platformio package (identical bits, esphome's own download machinery)."""

from __future__ import annotations

from collections.abc import Callable, Collection
from functools import cache, partial
import json
import logging
import os
from pathlib import Path
import platform
from typing import NamedTuple

from esphome.core import EsphomeError
from esphome.framework_helpers import (
    archive_extract_all,
    download_from_mirrors,
    download_with_resume,
    rmdir,
    run_batch_downloads,
)
from esphome.net_retry import fetch_with_retry, http_request

_LOGGER = logging.getLogger(__name__)

_REGISTRY_URL = (
    "https://api.registry.platformio.org/v3/packages/platformio/tool/{package}"
)


def get_systype() -> str:
    """The registry system tag for the current host.

    Transliterates ``platformio.util.get_systype()`` (same
    ``PLATFORMIO_SYSTEM_TYPE`` override). Deviation: windows-arm64 maps to
    ``windows_amd64`` (no arm64 toolchains; x86 emulation).
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


@cache
def registry_download(package: str, version: str) -> tuple[str, str, int | None]:
    """Resolve a package's download URL, sha256, and size via the registry.

    The metadata fetch goes through ``http_request``/``fetch_with_retry``
    (the consolidated HTTP path) so it shares the Happy Eyeballs patch and
    transient-retry policy of every other small fetch. Cached per process
    so the prefetch and the install resolve each package once (failures
    are not cached; the install retries them).
    """
    url = _REGISTRY_URL.format(package=package)

    def _fetch() -> str:
        resp = http_request("GET", url, timeout=30)
        resp.raise_for_status()
        return resp.text

    import requests

    try:
        body = fetch_with_retry(url, _fetch, what="Registry lookup")
    except requests.exceptions.RequestException as err:
        raise EsphomeError(
            f"Could not fetch registry metadata for {package}: {err}"
        ) from err
    try:
        data = json.loads(body)
    except ValueError as err:
        raise EsphomeError(
            f"The package registry returned invalid JSON for {package}: {err}"
        ) from err
    if not isinstance(data, dict):
        raise EsphomeError(
            f"Unexpected package registry response for {package}: {str(data)[:200]}"
        )
    systype = get_systype()
    versions = data.get("versions")
    if not isinstance(versions, list):
        # A schema change or an error/captive-portal payload must not be
        # reported as "version not found"
        raise EsphomeError(
            f"Unexpected package registry response for {package}: {str(data)[:200]}"
        )
    for ver in versions:
        if not isinstance(ver, dict):
            raise EsphomeError(
                f"Unexpected package registry response for {package}: {str(data)[:200]}"
            )
        if ver.get("name") != version:
            continue
        files = ver.get("files")
        if not isinstance(files, list):
            raise EsphomeError(
                f"Unexpected package registry response for {package}: {str(ver)[:200]}"
            )
        for file in files:
            if not isinstance(file, dict):
                raise EsphomeError(
                    f"Unexpected package registry response for {package}: "
                    f"{str(ver)[:200]}"
                )
            # Only a missing key means "any system"; an empty list must not
            # match, and a bare string would make ``in`` a substring test.
            systems = file.get("system")
            if systems is None:
                systems = ["*"]
            elif isinstance(systems, str):
                systems = [systems]
            elif not isinstance(systems, list):
                # An int would make ``in`` a TypeError and a dict a key test
                raise EsphomeError(
                    f"Unexpected package registry response for {package}: "
                    f"{str(file)[:200]}"
                )
            if "*" in systems or systype in systems:
                sha256 = (file.get("checksum") or {}).get("sha256")
                if not sha256:
                    # Never extract an unverified archive; the registry
                    # publishes a checksum for every package file.
                    raise EsphomeError(
                        f"The package registry returned no sha256 for "
                        f"{package} {version}; refusing the unverified download"
                    )
                url = file.get("download_url")
                if not url:
                    raise EsphomeError(
                        f"The package registry returned no download URL for "
                        f"{package} {version}"
                    )
                return (url, sha256, file.get("size"))
        raise EsphomeError(
            f"No {package} {version} build for this platform ({systype})"
        )
    raise EsphomeError(f"{package} {version} not found in the package registry")


def _check_layout(name: str, dest: Path, expect: Collection[str]) -> None:
    """Raise when an install tree is missing an expected directory (runs on
    fresh extracts and on marker hits)."""
    for rel in expect:
        if not (dest / rel).is_dir():
            raise EsphomeError(
                f"{name} at {dest} is missing the expected {rel} "
                "directory; run 'esphome clean-all' and retry"
            )


class _PendingArchive(NamedTuple):
    name: str
    version: str
    dest: Path
    url: str
    sha256: str
    size: int


def _already_installed(dest: Path) -> bool:
    """Whether ``dest`` holds a completed install (extraction marker)."""
    return (dest / ".esphome_extracted").is_file()


def prefetch_packages(
    packages: list[tuple[str, str, Path, list[str]]], downloads_dir: Path
) -> None:
    """Download pending package archives in parallel under one combined bar.

    ``packages`` holds ``(name, version, dest, mirrors)`` per package. Purely
    an optimization: ``install_package`` verifies every archive and
    re-downloads anything this pass left unfinished. Mirror overrides and
    registry entries without a size stay on the sequential path so its
    per-file bars remain trustworthy. Each fetch holds the same per-dest
    lock as ``install_package``: the archive's ``.part`` file is shared, and
    two concurrent writers would truncate each other's bytes.
    """
    from filelock import FileLock

    pending: list[_PendingArchive] = []
    seen: set[str] = set()
    for name, version, dest, mirrors in packages:
        if mirrors or (dest / ".esphome_extracted").is_file():
            continue
        archive_name = f"{name}-{version}"
        if archive_name in seen:
            # A duplicate entry would race itself between two workers
            continue
        seen.add(archive_name)
        try:
            url, sha256, size = registry_download(name, version)
        except EsphomeError as err:
            # The sequential install reports the real failure with context
            _LOGGER.debug("Prefetch resolve for %s failed: %s", name, err)
            continue
        if not size:
            continue
        archive = downloads_dir / archive_name
        if archive.is_file() and archive.stat().st_size == size:
            continue
        pending.append(_PendingArchive(name, version, dest, url, sha256, size))
    if len(pending) < 2:
        return
    downloads_dir.mkdir(parents=True, exist_ok=True)
    _LOGGER.info(
        "Downloading %d package archive(s): %s",
        len(pending),
        ", ".join(entry.name for entry in pending),
    )

    def _fetch(entry: _PendingArchive, tracker: Callable[[int], None]) -> None:
        entry.dest.parent.mkdir(parents=True, exist_ok=True)
        with FileLock(f"{entry.dest}.lock", fallback_to_soft=False):
            # Marker re-check: a concurrent build may have installed (and
            # deleted the archive of) this package while we waited;
            # re-downloading would orphan a fresh copy in downloads_dir
            # no branch: the thread tracer misses the skip edge; both
            # arms of _already_installed are pinned directly
            if not _already_installed(entry.dest):  # pragma: no branch
                download_with_resume(
                    entry.url,
                    downloads_dir / f"{entry.name}-{entry.version}",
                    sha256=entry.sha256,
                    size=entry.size,
                    progress=tracker,
                )

    failures = run_batch_downloads(
        "Downloading packages",
        [(entry.name, entry.size, partial(_fetch, entry)) for entry in pending],
    )
    for name, err in failures:
        if isinstance(err, (EsphomeError, OSError)):
            # Expected download failures: install_package retries this one
            # itself, with a visible bar
            _LOGGER.debug("Prefetch of %s failed: %s", name, err)
        else:
            # Anything else is a programming error that would otherwise
            # become a permanent silent no-op
            _LOGGER.warning("Prefetch of %s failed: %r", name, err, exc_info=err)


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
    if not expect:
        # Layout validation before marker.touch() is the only guard against
        # caching a truncated mirror archive as a good install
        raise ValueError("install_package requires a non-empty expect")
    marker = dest / ".esphome_extracted"
    if marker.is_file():
        _check_layout(name, dest, expect)
        return
    from filelock import FileLock

    # Serialize concurrent cold builds (same filelock pattern as git.py).
    dest.parent.mkdir(parents=True, exist_ok=True)
    # A soft-lock fallback would turn a hard-killed run into a permanent
    # hang (see git.py).
    with FileLock(f"{dest}.lock", fallback_to_soft=False):
        if marker.is_file():
            # Another process finished the install while we waited
            return
        rmdir(dest, msg=f"Clean up incomplete {name} install")
        # Persistent location so an interrupted download resumes across runs.
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
        _check_layout(name, dest, expect)
        marker.touch()
        archive.unlink(missing_ok=True)
