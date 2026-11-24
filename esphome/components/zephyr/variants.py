"""Shared code for the chips built with the Zephyr toolchain.

Each platform lists its chips in its own ``zephyr_variants`` package; a chip's module
holds its ``ZephyrVariant`` and its ``config_schema`` and ``to_code``.
"""

from dataclasses import dataclass
import logging
import re
from types import ModuleType

import esphome.config_validation as cv
from esphome.const import (
    CONF_FRAMEWORK,
    CONF_TYPE,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
)
from esphome.core import CORE
from esphome.types import ConfigType

from . import zephyr_data, zephyr_set_core_data
from .const import (
    KEY_BOOTLOADER,
    KEY_FAMILY,
    KEY_FRAMEWORK_TYPE,
    KEY_VARIANT,
    KEY_ZEPHYR,
    VERSION_RECOMMENDED,
)

_LOGGER = logging.getLogger(__name__)


@dataclass(frozen=True)
class ZephyrManifest:
    """The west manifest repository a build starts from (Zephyr, NCS, etc.)."""

    # The `framework: type:` value that selects this manifest
    framework_type: str
    default_version: str
    min_version: cv.Version


@dataclass(frozen=True, kw_only=True)
class ZephyrVariant:
    """What zephyr needs to know to build one chip."""

    # e.g. "STM32C5", the `variant:` value in the YAML
    name: str
    # The module sibling chips share, whose FAMILY is its name; None = no family
    family_module: ModuleType | None = None
    manifest: ZephyrManifest
    # Other manifests `framework: type:` may select
    alt_manifests: tuple[ZephyrManifest, ...] = ()

    @property
    def manifests(self) -> tuple[ZephyrManifest, ...]:
        """Its own manifest first, then the alternatives."""
        return (self.manifest, *self.alt_manifests)

    def manifest_for(self, framework_type: str | None) -> ZephyrManifest:
        """The manifest a `framework: type:` selects; None selects its own."""
        if framework_type is None:
            return self.manifest
        return {m.framework_type: m for m in self.manifests}[framework_type]


ZEPHYR_MAINLINE: ZephyrManifest = ZephyrManifest(
    framework_type="zephyr",
    default_version="4.5.0",
    min_version=cv.Version(4, 5, 0),
)


@dataclass(frozen=True)
class ZephyrVariantResolved:
    """A variant with the manifest and version its `framework:` block resolved to."""

    variant: ZephyrVariant
    manifest: ZephyrManifest
    version: cv.Version


def _release_order(version: cv.Version) -> tuple[int, int, int, bool, str, int]:
    """Sort key in which a release follows its prereleases and rc2 comes before rc10."""
    if (suffix := re.fullmatch(r"(\D*)(\d*)", version.extra)) is None:
        raise ValueError(f"Cannot order the version suffix {version.extra!r}")
    return (
        version.major,
        version.minor,
        version.patch,
        not version.extra,
        suffix[1],
        int(suffix[2] or 0),
    )


def resolve_variant(
    variant: ZephyrVariant, config: ConfigType, min_version_reason: str
) -> ZephyrVariantResolved:
    """Resolve `framework: type:` and `version:` of a variant's config.

    ``min_version_reason`` says why the manifest's minimum exists, e.g. "mainline STM32
    support".
    """
    framework = config[CONF_FRAMEWORK]
    framework_types = [m.framework_type for m in variant.manifests]
    framework_type = framework.get(CONF_TYPE)
    if framework_type is not None and framework_type not in framework_types:
        raise cv.Invalid(
            f"'{framework_type}' is not a valid framework type for {variant.name}. "
            f"Valid: {framework_types!r}",
            [CONF_FRAMEWORK, CONF_TYPE],
        )
    manifest = variant.manifest_for(framework_type)
    version_str = str(framework.get(CONF_VERSION, manifest.default_version))
    if version_str == VERSION_RECOMMENDED:
        version_str = manifest.default_version
    try:
        version = cv.Version.parse(version_str)
        version_order = _release_order(version)
    except ValueError as err:
        raise cv.Invalid(
            f"'{version_str}' is not a version number", [CONF_FRAMEWORK, CONF_VERSION]
        ) from err
    if version_order < _release_order(manifest.min_version):
        raise cv.Invalid(
            f"{variant.name} requires {manifest.framework_type} >= {manifest.min_version} "
            f"({min_version_reason}). Got {version_str}.",
            [CONF_FRAMEWORK, CONF_VERSION],
        )
    if version != cv.Version.parse(manifest.default_version):
        _LOGGER.warning(
            "The selected %s version is not the recommended one (%s). "
            "If there are connectivity or build issues please remove the manual version.",
            manifest.framework_type,
            manifest.default_version,
        )
    return ZephyrVariantResolved(variant, manifest, version)


def set_variant_core_data(
    resolved: ZephyrVariantResolved,
    config: ConfigType,
    *,
    bootloader: str,
) -> None:
    """Fill CORE.data for a variant's config_schema; ``bootloader`` is "" for none."""
    zephyr_set_core_data({**config, KEY_BOOTLOADER: bootloader})
    variant = resolved.variant
    zephyr_data()[KEY_VARIANT] = variant.name
    zephyr_data()[KEY_FAMILY] = (
        variant.family_module.FAMILY if variant.family_module else None
    )
    zephyr_data()[KEY_FRAMEWORK_TYPE] = resolved.manifest.framework_type
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = resolved.version
