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
    """URIs of the package versions an ESPHome build installs for LibreTiny.

    The platform picks versions by chip family, so several versions of one
    package are in use at the same time. This is each family's default plus
    the versions ESPHome selects itself, in the form PlatformIO records for
    an installed package; a version only a user's own ``custom_versions``
    selects is not included. None if the manifest cannot be read.
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
                # A mapping offers alternatives that only a user's own
                # custom_versions can select; the build takes "default"
                if not isinstance(versions, str):
                    versions = versions.get("default")
                if versions is not None:
                    urls.add(_versioned(packages[f"library-{library}"], versions))
        for name, version in CUSTOM_VERSIONS.items():
            for kind in ("library", "framework", "tool"):
                if (target := packages.get(f"{kind}-{name}")) is not None:
                    urls.add(_versioned(target, version))
                    break
    except (OSError, ValueError, LookupError, TypeError, AttributeError) as err:
        _LOGGER.debug("Could not read the package versions of %s: %s", manifest, err)
        return None
    return {PackageSpec(url).uri for url in urls}
