"""Registry of the chips built with the Zephyr toolchain.

Each platform lists its chips in its own ``zephyr_variants`` package; a chip's module
holds its ``ZephyrVariant`` and its ``config_schema`` and ``to_code``.
"""

from collections.abc import Iterator, Mapping
from dataclasses import dataclass, field
import importlib
import logging
from types import ModuleType

import esphome.config_validation as cv
from esphome.const import (
    CONF_FRAMEWORK,
    CONF_TYPE,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    PLATFORM_STM32,
)
from esphome.core import CORE
from esphome.types import ConfigType

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
    default_version: str
    min_version: cv.Version


@dataclass
class ZephyrVariant:
    sdk: ZephyrSDK
    # The `framework: type:` value naming `sdk`
    sdk_name: str = "zephyr"
    # Other SDKs `framework: type:` may select, by type name
    alt_sdks: dict[str, ZephyrSDK] = field(default_factory=dict)
    # Sibling chips that share a module, ``<family>_family`` next to the variants
    family: str | None = None


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
    except ValueError as err:
        raise cv.Invalid(
            f"'{version_str}' is not a version number", [CONF_FRAMEWORK, CONF_VERSION]
        ) from err
    if framework_ver < sdk.min_version:
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
    config: ConfigType,
    framework_ver: cv.Version,
    *,
    framework_type: str,
    bootloader: str,
) -> None:
    """Fill CORE.data for a variant's config_schema; ``bootloader`` is "" for none."""
    # deferred: zephyr's __init__ may import this package
    from .. import zephyr_data, zephyr_set_core_data

    zephyr_set_core_data({**config, KEY_BOOTLOADER: bootloader})
    zephyr_data()[KEY_VARIANT] = variant_name
    zephyr_data()[KEY_FAMILY] = VARIANTS[variant_name].family
    zephyr_data()[KEY_FRAMEWORK_TYPE] = framework_type
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = framework_ver


# Platforms whose `zephyr_variants` package lists its variant modules in VARIANT_MODULES
_ZEPHYR_PLATFORMS: tuple[str, ...] = (PLATFORM_STM32,)


class _LazyVariants(Mapping[str, ZephyrVariant]):
    """Loads the variant modules on first use; they import this package."""

    _built: bool = False
    _loading: bool = False
    _variants: dict[str, ZephyrVariant]
    _modules: dict[str, ModuleType]
    _families: dict[str, ModuleType]

    def __init__(self) -> None:
        self._variants = {}
        self._modules = {}
        self._families = {}

    def _ensure(self) -> None:
        if self._built:
            return
        if self._loading:
            raise RuntimeError("A Zephyr variant module read VARIANTS while it loaded")
        self._loading = True
        try:
            self._load()
        except BaseException:
            # Raise the import error again next time, not a KeyError on a partial registry
            self._variants.clear()
            self._modules.clear()
            self._families.clear()
            raise
        finally:
            self._loading = False
        self._built = True

    def _load(self) -> None:
        for platform in _ZEPHYR_PLATFORMS:
            package = importlib.import_module(
                f"esphome.components.{platform}.zephyr_variants"
            )
            for mod_name in package.VARIANT_MODULES:
                mod = importlib.import_module(f".{mod_name}", package.__name__)
                if (other := self._modules.get(mod.VARIANT_NAME)) is not None:
                    raise ValueError(
                        f"{mod.__name__} and {other.__name__} are both {mod.VARIANT_NAME!r}"
                    )
                self._variants[mod.VARIANT_NAME] = mod.VARIANT
                self._modules[mod.VARIANT_NAME] = mod
                if (family := mod.VARIANT.family) and family not in self._families:
                    self._families[family] = importlib.import_module(
                        f".{family}_family", package.__name__
                    )

    def module(self, key: str) -> ModuleType:
        self._ensure()
        return self._modules[key]

    def family_module(self, family: str) -> ModuleType:
        self._ensure()
        if (mod := self._families.get(family)) is None:
            raise ValueError(f"Unknown Zephyr family: {family!r}")
        return mod

    def __getitem__(self, key: str) -> ZephyrVariant:
        self._ensure()
        return self._variants[key]

    def __iter__(self) -> Iterator[str]:
        self._ensure()
        return iter(self._variants)

    def __len__(self) -> int:
        self._ensure()
        return len(self._variants)


_variants_instance: _LazyVariants = _LazyVariants()
VARIANTS: Mapping[str, ZephyrVariant] = _variants_instance


def get_variant_module(variant: str) -> ModuleType:
    """The variant's own module, which holds its config_schema and to_code."""
    return _variants_instance.module(variant)


def get_family_module(family: str) -> ModuleType:
    return _variants_instance.family_module(family)
