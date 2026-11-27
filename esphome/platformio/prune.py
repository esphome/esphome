"""Remove what PlatformIO installed for platforms that no longer build with it."""

from __future__ import annotations

from collections.abc import Collection, Mapping
import json
import logging
from pathlib import Path
import sys
from typing import NamedTuple

from esphome.build_helpers.prune import (
    installed_name,
    is_leftover,
    remove_trees,
    subdirs,
    with_leftover,
)
from esphome.build_helpers.tools_cache import IDF_TOOLS_CACHE, tools_cache_path
from esphome.framework_helpers import get_python_env_executable_path
from esphome.platformio.toolchain import get_platformio_config

_LOGGER = logging.getLogger(__name__)

ESPRESSIF32 = "espressif32"
ESPRESSIF8266 = "espressif8266"
NORDICNRF52 = "nordicnrf52"
RASPBERRYPI = "raspberrypi"
LIBRETINY = "libretiny"
# The platforms whose PlatformIO toolchain is deprecated
DEPRECATED_PLATFORMS = (ESPRESSIF32, ESPRESSIF8266, NORDICNRF52)
# A package installed from a URL is moved aside to "<name>@src-<hash>" when
# another install takes its name. LibreTiny's chip families build with
# different versions of one package at once, so such a copy may still be in use.
_REPLACED_MARK = "@src-"
# What PlatformIO names an install it has not finished
_INSTALLING_PREFIX = "_tmp_installing"
# pioarduino's espressif32 installs its ESP-IDF tools, and keeps their
# downloads and records, directly in PlatformIO's core directory
_ESPRESSIF32_CORE_ENTRIES = ("tools", "dist", "root_managed_components", "idf-env.json")
_PENV = "penv"
_PENV_PIOARDUINO_TOOL = "esptool"
_PENV_ESPIDF_PREFIX = ".espidf-"


def _name(path: Path) -> str:
    """The platform or package name of ``<name>`` or ``<name>@<version>``."""
    return installed_name(path).partition("@")[0]


def _espressif32_python_environments(core_dir: Path) -> list[Path]:
    """What the ESP32 platforms left in PlatformIO's ``penv`` directory.

    That directory is shared. pioarduino makes it its own environment, and is
    the only one to put esptool in it. Both it and the official platform keep
    one environment per ESP-IDF version in ``.espidf-<version>``. Others keep
    theirs in a hidden folder of their own (LibreTiny in ``.libretiny``), and
    PlatformIO's own installer puts PlatformIO itself there. Only what the
    ESP32 platforms made is removed; the directory goes with it when nothing
    else is in it. An environment ESPHome itself is running from is left alone.
    """
    penv = core_dir / _PENV
    removed = [
        path for path in with_leftover(penv, platformio=True) if is_leftover(path)
    ]
    if not penv.is_dir() or penv.is_symlink():
        return removed
    if Path(sys.prefix).resolve().is_relative_to(penv.resolve()):
        return removed
    tool = get_python_env_executable_path(penv, _PENV_PIOARDUINO_TOOL)
    pioarduino = tool.is_file()
    entries = sorted(penv.iterdir())
    doomed = [
        entry
        for entry in entries
        if is_leftover(entry)
        or entry.name.startswith(_PENV_ESPIDF_PREFIX)
        or (pioarduino and not (entry.name.startswith(".") and entry.is_dir()))
    ]
    if entries and len(doomed) == len(entries):
        return [*removed, penv]
    # The folder with esptool goes last: it is what tells a prune that
    # continues an interrupted one that the rest is pioarduino's
    doomed.sort(key=lambda entry: entry == tool.parent)
    return removed + doomed


class _Listed(NamedTuple):
    """How a platform lists a package."""

    optional: bool
    # The URL the package is installed from, None if it comes by version
    url: str | None


def _packages(platform: Path) -> dict[str, _Listed] | None:
    """The packages a platform lists, None if they cannot be read."""
    manifest = platform / "platform.json"
    try:
        packages = json.loads(manifest.read_text(encoding="utf-8"))["packages"]
        return {
            name: _Listed(
                bool(package.get("optional")),
                version
                if isinstance(version := package.get("version"), str)
                and "://" in version
                else None,
            )
            for name, package in packages.items()
        }
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as err:
        _LOGGER.debug("Could not read the packages of %s: %s", manifest, err)
        return None


def _installed_from(install: Path) -> str | None:
    """The URI PlatformIO recorded for an install, None if unknown."""
    # An install cloned with git keeps its record inside .git
    for record in (install / ".git" / ".piopm", install / ".piopm"):
        try:
            uri = json.loads(record.read_text(encoding="utf-8"))["spec"]["uri"]
        except FileNotFoundError:
            continue
        except (OSError, ValueError, KeyError, TypeError) as err:
            _LOGGER.debug("Could not read %s: %s", record, err)
            return None
        return uri if isinstance(uri, str) else None
    return None


def _unused_replaced_packages(
    packages_dir: Path,
    names: Collection[str],
    uris: Collection[str],
    libretiny: list[Path],
    shared: Collection[str],
) -> list[Path]:
    """Replaced copies of packages that no platform that stays installs.

    ``names`` are the packages a removed platform lists and ``uris`` what the
    platforms that stay list a URL for. ``libretiny`` are the LibreTiny
    platforms that stay: copies of their packages in a version they no longer
    build with go too, except of the ``shared`` ones, which PlatformIO itself
    or another platform that stays lists.
    """
    from esphome.components.libretiny.prune import package_uris

    names = set(names)
    uris = set(uris)
    for platform in libretiny:
        listed = _packages(platform)
        used = package_uris(platform)
        if listed is None or used is None:
            _LOGGER.warning(
                "Keeping all replaced packages, could not tell which versions "
                "%s builds with",
                platform,
            )
            return []
        names |= listed.keys() - set(shared)
        uris |= used
    return [
        package
        for package in subdirs(packages_dir)
        if _REPLACED_MARK in package.name
        and _name(package) in names
        and (uri := _installed_from(package)) is not None
        and uri not in uris
    ]


def prune_platformio_installs(
    platforms: Collection[str],
    recommended: Mapping[str, str] | None = None,
    dry_run: bool = False,
) -> list[Path]:
    """Remove the named PlatformIO platforms and the packages only they use.

    ``recommended`` maps the name of a platform that still builds with
    PlatformIO to the source its recommended version is installed from. Of
    such a platform, every install from another source is removed too, and so
    are the replaced LibreTiny packages in a version it no longer builds
    with. An install whose source is not recorded is kept.

    A package that PlatformIO itself or a remaining platform needs is kept,
    but not a replaced copy of it that only a named platform installed.
    PlatformIO's download cache is cleared. Returns what was removed (what
    would be removed on a dry run). Does nothing when PlatformIO is not
    installed.
    """
    if (config := get_platformio_config()) is None:
        return []
    from platformio.dependencies import get_core_dependencies
    from platformio.package.meta import PackageSpec

    # In the form PlatformIO records for an install
    sources = {
        name: PackageSpec(source).uri for name, source in (recommended or {}).items()
    }

    def other_source(platform: Path) -> bool:
        source = sources.get(_name(platform))
        installed_from = _installed_from(platform)
        return (
            source is not None
            and installed_from is not None
            and installed_from != source
        )

    doomed: list[Path] = []
    kept: list[Path] = []
    platforms_dir = Path(config.get("platformio", "platforms_dir"))
    for platform in subdirs(platforms_dir, symlinks=True):
        if platform.name.startswith(_INSTALLING_PREFIX) and not is_leftover(platform):
            continue
        if is_leftover(platform) or (
            not platform.is_symlink()
            and (_name(platform) in platforms or other_source(platform))
        ):
            doomed.append(platform)
        else:
            kept.append(platform)

    core_packages = set(get_core_dependencies())
    # Package name -> the removed platforms that list it
    owners: dict[str, set[str]] = {}
    for platform in doomed:
        if (packages := _packages(platform)) is None:
            _LOGGER.warning(
                "Could not tell which packages %s installed, removing it without them",
                platform,
            )
            continue
        for package in packages:
            owners.setdefault(package, set()).add(_name(platform))
    for package in core_packages:
        owners.pop(package, None)
    # A replaced copy of one of these may be a deprecated platform's own, even
    # when the package itself stays. The nRF52 and LibreTiny toolchains share
    # a name this way.
    replaced = {package for package, names in owners.items() if names & set(platforms)}
    # Package name -> the platforms that stay and list it
    listed_by: dict[str, set[str]] = {}
    # What the platforms that stay list a URL for
    uris: set[str] = set()
    all_read = True
    for platform in kept:
        if (packages := _packages(platform)) is None:
            _LOGGER.warning(
                "Keeping all PlatformIO platforms and packages, could not tell "
                "which packages %s uses",
                platform,
            )
            all_read = False
            owners = {}
            break
        name = _name(platform)
        for package, listed in packages.items():
            listed_by.setdefault(package, set()).add(name)
            if listed.url is not None:
                uris.add(PackageSpec(listed.url).uri)
            if name in platforms:
                replaced.discard(package)
            if not listed.optional or name in platforms:
                owners.pop(package, None)
            elif package in owners:
                # Optional here: kept if only this platform's own removed
                # installs list it, removed if another removed platform does.
                # The RP2 platform lists cmake and ninja this way, for a
                # framework ESPHome never builds with.
                owners[package].discard(name)
    owned = {package for package, names in owners.items() if names}
    if not all_read:
        # Once a platform is gone nothing says which packages were its own
        kept += [platform for platform in doomed if not is_leftover(platform)]
        doomed = [platform for platform in doomed if is_leftover(platform)]

    # Packages go before the platforms: the platforms say which packages are theirs
    packages_dir = Path(config.get("platformio", "packages_dir"))
    removed = [
        package
        for package in subdirs(packages_dir)
        if is_leftover(package) or _name(package) in owned
    ]
    if all_read:
        shared = core_packages | {
            package for package, names in listed_by.items() if names != {LIBRETINY}
        }
        libretiny = [platform for platform in kept if _name(platform) == LIBRETINY]
        removed += [
            package
            for package in _unused_replaced_packages(
                packages_dir, replaced, uris, libretiny, shared
            )
            if package not in removed
        ]
    # The downloads PlatformIO keeps of what it installed; `esphome clean`
    # clears this too. PlatformIO makes the directory again, so an empty one stays.
    cache_dir = Path(config.get("platformio", "cache_dir"))
    removed += [
        path
        for path in with_leftover(cache_dir, platformio=True)
        if is_leftover(path) or (path.is_dir() and any(path.iterdir()))
    ]
    core_dir = Path(config.get("platformio", "core_dir"))
    if (
        ESPRESSIF32 in platforms
        and ESPRESSIF32 not in map(_name, kept)
        # The native ESP-IDF install can be pointed at the same directory;
        # the tools in it are then that install's
        and tools_cache_path(*IDF_TOOLS_CACHE) != core_dir.resolve()
    ):
        for name in _ESPRESSIF32_CORE_ENTRIES:
            removed += with_leftover(core_dir / name, platformio=True)
        removed += _espressif32_python_environments(core_dir)
    removed += doomed

    remove_trees(removed, dry_run, platformio=True)
    return removed
