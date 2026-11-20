"""Shared code for the chips built with the Zephyr toolchain.

Each platform lists its chips in its own ``zephyr_variants`` package; a chip's module
holds its ``ZephyrVariant`` and its ``config_schema`` and ``to_code``.
"""

from dataclasses import dataclass, field
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

from .. import zephyr_data, zephyr_set_core_data
from ..const import (
    KEY_BOOTLOADER,
    KEY_FAMILY,
    KEY_FRAMEWORK_TYPE,
    KEY_VARIANT,
    KEY_ZEPHYR,
    VERSION_RECOMMENDED,
)

_LOGGER = logging.getLogger(__name__)


@dataclass
class ZephyrSDK:
    """A framework distribution (Zephyr, NCS, etc.), not the Zephyr SDK toolchain."""

    default_version: str
    min_version: cv.Version


@dataclass
class ZephyrVariant:
    sdk: ZephyrSDK
    # The `framework: type:` value naming `sdk`
    sdk_name: str = "zephyr"
    # Other SDKs `framework: type:` may select, by type name
    alt_sdks: dict[str, ZephyrSDK] = field(default_factory=dict)
    # The module sibling chips share, whose FAMILY is its name; None = no family
    family: ModuleType | None = None


ZEPHYR_MAINLINE: ZephyrSDK = ZephyrSDK(
    default_version="4.5.0",
    min_version=cv.Version(4, 5, 0),
)


def resolve_sdk(
    variant: ZephyrVariant, framework_type: str | None
) -> tuple[str, ZephyrSDK]:
    """The (type name, SDK) a `framework: type:` selects; None selects the variant's own."""
    if framework_type is None or framework_type == variant.sdk_name:
        return variant.sdk_name, variant.sdk
    return framework_type, variant.alt_sdks[framework_type]


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


def resolve_framework_version(
    variant: ZephyrVariant, variant_name: str, config: ConfigType, requires_reason: str
) -> tuple[str, cv.Version, str, ZephyrSDK]:
    """Resolve `framework: type:` and `version:` of a variant's config.

    ``requires_reason`` says why the SDK's minimum exists, e.g. "mainline STM32 support".
    Returns the version string, the parsed version, the type name and its SDK.
    """
    framework = config[CONF_FRAMEWORK]
    type_names = [variant.sdk_name, *variant.alt_sdks]
    framework_type = framework.get(CONF_TYPE)
    if framework_type is not None and framework_type not in type_names:
        raise cv.Invalid(
            f"'{framework_type}' is not a valid framework type for {variant_name}. "
            f"Valid: {type_names!r}",
            [CONF_FRAMEWORK, CONF_TYPE],
        )
    sdk_name, sdk = resolve_sdk(variant, framework_type)
    version_str = str(framework.get(CONF_VERSION, sdk.default_version))
    if version_str == VERSION_RECOMMENDED:
        version_str = sdk.default_version
    try:
        framework_ver = cv.Version.parse(version_str)
        framework_order = _release_order(framework_ver)
    except ValueError as err:
        raise cv.Invalid(
            f"'{version_str}' is not a version number", [CONF_FRAMEWORK, CONF_VERSION]
        ) from err
    if framework_order < _release_order(sdk.min_version):
        raise cv.Invalid(
            f"{variant_name} requires {sdk_name} >= {sdk.min_version} "
            f"({requires_reason}). Got {version_str}.",
            [CONF_FRAMEWORK, CONF_VERSION],
        )
    if framework_ver != cv.Version.parse(sdk.default_version):
        _LOGGER.warning(
            "The selected %s version is not the recommended one (%s). "
            "If there are connectivity or build issues please remove the manual version.",
            sdk_name,
            sdk.default_version,
        )
    return version_str, framework_ver, sdk_name, sdk


def set_core_data(
    variant_name: str,
    variant: ZephyrVariant,
    config: ConfigType,
    framework_ver: cv.Version,
    *,
    framework_type: str,
    bootloader: str,
) -> None:
    """Fill CORE.data for a variant's config_schema; ``bootloader`` is "" for none."""
    zephyr_set_core_data({**config, KEY_BOOTLOADER: bootloader})
    zephyr_data()[KEY_VARIANT] = variant_name
    zephyr_data()[KEY_FAMILY] = variant.family.FAMILY if variant.family else None
    zephyr_data()[KEY_FRAMEWORK_TYPE] = framework_type
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = framework_ver
