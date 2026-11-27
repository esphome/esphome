"""Remove what PlatformIO installed for platforms that no longer build with it."""

from __future__ import annotations

from collections.abc import Collection
import json
import logging
from pathlib import Path

from esphome.build_helpers.prune import (
    PRUNING_SUFFIX,
    is_leftover,
    remove_trees,
    subdirs,
)
from esphome.platformio.toolchain import get_platformio_config

_LOGGER = logging.getLogger(__name__)

ESPRESSIF32 = "espressif32"
ESPRESSIF8266 = "espressif8266"
NORDICNRF52 = "nordicnrf52"
LIBRETINY = "libretiny"
# The platforms whose PlatformIO toolchain is deprecated
DEPRECATED_PLATFORMS = (ESPRESSIF32, ESPRESSIF8266, NORDICNRF52)
# Installed from a URL that changes with every version. PlatformIO moves the
# copy a new install replaces to "<name>@src-<hash>", so a directory of that
# form is never the one in use. The RP2 packages need no such rule, a new
# version overwrites them in place. LibreTiny's packages are moved aside too,
# but its chip families build with different versions of one package at once,
# so those copies are checked one by one (_unused_libretiny_packages).
_SUPERSEDED_PLATFORMS = ("raspberrypi", LIBRETINY)
_SUPERSEDED_MARK = "@src-"
# pioarduino's espressif32 installs its ESP-IDF tools, and keeps their
# downloads, directly in PlatformIO's core directory
_ESPRESSIF32_CORE_DIRS = ("tools", "dist")


def _name(path: Path) -> str:
    """The platform or package name of ``<name>`` or ``<name>@<version>``."""
    return path.name.removesuffix(PRUNING_SUFFIX).partition("@")[0]


def _superseded(path: Path, names: Collection[str]) -> bool:
    return _SUPERSEDED_MARK in path.name and _name(path) in names


def _packages(platform: Path) -> dict[str, bool] | None:
    """The packages a platform lists and whether it marks each one optional.

    None if they cannot be read.
    """
    manifest = platform / "platform.json"
    try:
        packages = json.loads(manifest.read_text(encoding="utf-8"))["packages"]
        return {
            name: bool(package.get("optional")) for name, package in packages.items()
        }
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as err:
        _LOGGER.debug("Could not read the packages of %s: %s", manifest, err)
        return None


def _installed_from(package: Path) -> str | None:
    """The URI PlatformIO recorded for an installed package, None if unknown."""
    # A package cloned with git keeps its record inside .git
    for record in (package / ".git" / ".piopm", package / ".piopm"):
        try:
            return json.loads(record.read_text(encoding="utf-8"))["spec"]["uri"]
        except FileNotFoundError:
            continue
        except (OSError, ValueError, KeyError, TypeError) as err:
            _LOGGER.debug("Could not read %s: %s", record, err)
            return None
    return None


def _unused_libretiny_packages(platforms_dir: Path, packages_dir: Path) -> list[Path]:
    """Replaced copies of LibreTiny packages in a version it no longer builds with."""
    from esphome.components.libretiny.prune import package_uris

    platform = platforms_dir / LIBRETINY
    if (names := _packages(platform)) is None:
        return []
    if (uris := package_uris(platform)) is None:
        return []
    return [
        package
        for package in subdirs(packages_dir)
        if _superseded(package, names)
        and (uri := _installed_from(package)) is not None
        and uri not in uris
    ]


def prune_platformio_installs(
    platforms: Collection[str], dry_run: bool = False
) -> list[Path]:
    """Remove the named PlatformIO platforms and the packages only they use.

    A package that PlatformIO itself or a remaining platform needs is kept.
    The copies of the RP2 and LibreTiny platforms that a newer install
    replaced are removed too, and so are the replaced LibreTiny packages in a
    version it no longer builds with.
    Returns the removed directories (the ones that would be removed on a dry
    run). Does nothing when PlatformIO is not installed.
    """
    if (config := get_platformio_config()) is None:
        return []
    from platformio.dependencies import get_core_dependencies

    doomed: list[Path] = []
    kept: list[Path] = []
    platforms_dir = Path(config.get("platformio", "platforms_dir"))
    for platform in subdirs(platforms_dir, symlinks=True):
        if is_leftover(platform) or (
            not platform.is_symlink()
            and (
                _name(platform) in platforms
                or _superseded(platform, _SUPERSEDED_PLATFORMS)
            )
        ):
            doomed.append(platform)
        else:
            kept.append(platform)

    # Package name -> the removed platforms that list it
    owners: dict[str, set[str]] = {}
    for platform in doomed:
        for package in _packages(platform) or ():
            owners.setdefault(package, set()).add(_name(platform))
    for package in get_core_dependencies():
        owners.pop(package, None)
    for platform in kept:
        if (packages := _packages(platform)) is None:
            _LOGGER.warning(
                "Keeping all PlatformIO packages, could not tell which ones %s uses",
                platform,
            )
            owners = {}
            break
        name = _name(platform)
        for package, optional in packages.items():
            if not optional or name in platforms:
                owners.pop(package, None)
            elif package in owners:
                # Optional here: kept if only this platform's own replaced
                # copies list it, removed if another removed platform does.
                # The RP2 platform lists cmake and ninja this way, for a
                # framework ESPHome never builds with.
                owners[package].discard(name)
    owned = {package for package, names in owners.items() if names}

    # Packages go before the platforms: the platforms say which packages are theirs
    packages_dir = Path(config.get("platformio", "packages_dir"))
    removed = [
        package
        for package in subdirs(packages_dir)
        if is_leftover(package) or _name(package) in owned
    ]
    removed += [
        package
        for package in _unused_libretiny_packages(platforms_dir, packages_dir)
        if package not in removed
    ]
    if ESPRESSIF32 in platforms and ESPRESSIF32 not in map(_name, kept):
        core_dir = Path(config.get("platformio", "core_dir"))
        removed += [
            path
            for name in _ESPRESSIF32_CORE_DIRS
            if (path := core_dir / name).is_dir() and not path.is_symlink()
        ]
    removed += doomed

    remove_trees(removed, dry_run)
    return removed
