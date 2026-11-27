"""Which package versions the installed LibreTiny platform builds with."""

from __future__ import annotations

import json
import logging
from pathlib import Path
from typing import Any

from .const import CUSTOM_VERSIONS

_LOGGER = logging.getLogger(__name__)


def _versioned(package: dict[str, Any], version: str) -> str:
    """The URL the platform installs ``version`` of ``package`` from."""
    base = package.get("base_url") or package["version"].partition("#")[0]
    prefix = "v" if package.get("version_prefix") else ""
    return f"{base}#{prefix}{version}"


def package_uris(platform: Path) -> set[str] | None:
    """URIs of every package version the LibreTiny platform can install.

    The platform picks versions by chip family and by ``custom_versions``, so
    several versions of one package are in use at the same time. This is
    every version its manifest lists plus the ones ESPHome selects itself, in
    the form PlatformIO records for an installed package. None if the manifest
    cannot be read.
    """
    from platformio.package.meta import PackageSpec

    manifest = platform / "platform.json"
    try:
        packages = json.loads(manifest.read_text(encoding="utf-8"))["packages"]
        urls = {
            version
            for package in packages.values()
            if "://" in (version := package.get("version", ""))
        }
        for package in packages.values():
            for library, versions in package.get("libraries", {}).items():
                target = packages[f"library-{library}"]
                listed = [versions] if isinstance(versions, str) else versions.values()
                urls |= {_versioned(target, version) for version in listed}
        for name, version in CUSTOM_VERSIONS.items():
            for kind in ("library", "framework", "tool"):
                if (target := packages.get(f"{kind}-{name}")) is not None:
                    urls.add(_versioned(target, version))
                    break
    except (OSError, ValueError, LookupError, TypeError, AttributeError) as err:
        _LOGGER.debug("Could not read the package versions of %s: %s", manifest, err)
        return None
    return {PackageSpec(url).uri for url in urls}
