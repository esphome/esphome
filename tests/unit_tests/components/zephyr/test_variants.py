"""Tests for the shared Zephyr variant code."""

from collections.abc import Callable
from importlib import import_module
import logging
import pkgutil
from types import ModuleType

import pytest

from esphome.components.zephyr import (
    zephyr_framework_type,
    zephyr_manifest,
    zephyr_set_core_data,
    zephyr_variant,
    zephyr_variant_family_module,
    zephyr_variant_family_name,
    zephyr_variant_name,
)
from esphome.components.zephyr.const import KEY_BOOTLOADER
from esphome.components.zephyr.variants import (
    ZEPHYR_MAINLINE,
    ZephyrManifest,
    ZephyrVariant,
    ZephyrVariantResolved,
    resolve_variant,
    set_variant_core_data,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_TYPE,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_NRF52,
    PLATFORM_STM32,
)
from esphome.core import CORE

_MANIFEST: ZephyrManifest = ZephyrManifest(
    framework_type="zephyr",
    default_version="4.5.0",
    min_version=cv.Version(4, 4, 0),
)
_FAMILY_MODULE: ModuleType = ModuleType("chipfam_family")
_FAMILY_MODULE.FAMILY = "chipfam"
_VARIANT: ZephyrVariant = ZephyrVariant(
    name="chip1", family_module=_FAMILY_MODULE, manifest=_MANIFEST
)
_VENDOR_MANIFEST: ZephyrManifest = ZephyrManifest(
    framework_type="vendor",
    default_version="2.0.0",
    min_version=cv.Version(2, 0, 0),
)
_ALT_VARIANT: ZephyrVariant = ZephyrVariant(
    name="chip1", manifest=_MANIFEST, alt_manifests=(_VENDOR_MANIFEST,)
)


def _config(
    version: str | None = None, framework_type: str | None = None
) -> dict[str, object]:
    framework: dict[str, str] = {}
    if version is not None:
        framework[CONF_VERSION] = version
    if framework_type is not None:
        framework[CONF_TYPE] = framework_type
    return {CONF_BOARD: "chip1_dk", KEY_BOOTLOADER: "", CONF_FRAMEWORK: framework}


def test_zephyr_mainline_default_is_its_minimum() -> None:
    assert (
        cv.Version.parse(ZEPHYR_MAINLINE.default_version) == ZEPHYR_MAINLINE.min_version
    )


@pytest.mark.parametrize("version", [None, "recommended", "4.5.0"])
@pytest.mark.parametrize("framework_type", [None, "zephyr"])
def test_version_defaults_to_the_manifest_default(
    version: str | None, framework_type: str | None, caplog: pytest.LogCaptureFixture
) -> None:
    assert resolve_variant(
        _VARIANT, _config(version, framework_type), "chip1 support"
    ) == ZephyrVariantResolved(_VARIANT, _MANIFEST, cv.Version(4, 5, 0))
    assert "not the recommended one" not in caplog.text


def test_type_selects_an_alternative_manifest() -> None:
    assert resolve_variant(
        _ALT_VARIANT, _config(framework_type="vendor"), "chip1 support"
    ) == ZephyrVariantResolved(_ALT_VARIANT, _VENDOR_MANIFEST, cv.Version(2, 0, 0))


def test_unknown_type_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match=r"Valid: \['zephyr', 'vendor'\]") as err:
        resolve_variant(_ALT_VARIANT, _config(framework_type="other"), "chip1 support")
    assert err.value.path == [CONF_FRAMEWORK, CONF_TYPE]


def test_version_other_than_the_default_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level(logging.WARNING):
        resolved = resolve_variant(_VARIANT, _config("4.4.0"), "chip1 support")
    assert resolved.version == cv.Version(4, 4, 0)
    assert "not the recommended one (4.5.0)" in caplog.text


@pytest.mark.parametrize("version", ["v4.5.0", "4.5.0-rc1a"])
def test_version_that_is_not_a_number_is_rejected(version: str) -> None:
    with pytest.raises(cv.Invalid, match=f"'{version}' is not a version number") as err:
        resolve_variant(_VARIANT, _config(version), "chip1 support")
    assert err.value.path == [CONF_FRAMEWORK, CONF_VERSION]


@pytest.mark.parametrize("version", ["4.3.0", "4.4.0-rc1"])
def test_version_below_the_minimum_is_rejected(version: str) -> None:
    """A prerelease of the minimum is below it too."""
    with pytest.raises(cv.Invalid, match=r"zephyr >= 4.4.0 \(chip1 support\)") as err:
        resolve_variant(_VARIANT, _config(version), "chip1 support")
    assert err.value.path == [CONF_FRAMEWORK, CONF_VERSION]


def test_release_meets_a_prerelease_minimum() -> None:
    manifest = ZephyrManifest(
        framework_type="zephyr",
        default_version="4.5.0",
        min_version=cv.Version(4, 5, 0, "rc2"),
    )
    variant = ZephyrVariant(name="chip1", manifest=manifest)
    for version in ("4.5.0", "4.5.0-rc10"):
        resolve_variant(variant, _config(version), "chip1 support")


@pytest.mark.parametrize(
    ("variant", "manifest", "family"),
    [(_VARIANT, _MANIFEST, "chipfam"), (_ALT_VARIANT, _VENDOR_MANIFEST, None)],
)
def test_set_variant_core_data(
    variant: ZephyrVariant, manifest: ZephyrManifest, family: str | None
) -> None:
    CORE.data[KEY_CORE] = {}

    config = {CONF_BOARD: "chip1_dk", CONF_FRAMEWORK: {}}
    resolved = ZephyrVariantResolved(variant, manifest, cv.Version(4, 5, 0))
    set_variant_core_data(resolved, config, bootloader="mcuboot")

    assert zephyr_variant_name() == "chip1"
    assert zephyr_variant_family_name() == family
    assert zephyr_framework_type() == manifest.framework_type
    assert zephyr_manifest() is manifest
    assert zephyr_variant() is variant
    assert zephyr_variant_family_module() is variant.family_module
    assert CORE.data["zephyr"][CONF_BOARD] == "chip1_dk"
    assert CORE.data["zephyr"][KEY_BOOTLOADER] == "mcuboot"
    assert CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] == "zephyr"
    assert CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] == cv.Version(4, 5, 0)


_LOOKUPS: tuple[Callable[[], object], ...] = (
    zephyr_variant_name,
    zephyr_variant_family_name,
    zephyr_framework_type,
    zephyr_manifest,
    zephyr_variant,
    zephyr_variant_family_module,
)


def test_no_variant_on_nrf52_or_other_platforms() -> None:
    """nrf52 sets up Zephyr data without a variant; other platforms have none."""
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_ESP32,
        KEY_TARGET_FRAMEWORK: "esp-idf",
    }
    assert [lookup() for lookup in _LOOKUPS] == [None] * len(_LOOKUPS)
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_NRF52,
        KEY_TARGET_FRAMEWORK: "zephyr",
    }
    zephyr_set_core_data(_config())
    assert [lookup() for lookup in _LOOKUPS] == [None] * len(_LOOKUPS)


@pytest.mark.parametrize("platform", [None, PLATFORM_ESP32, PLATFORM_STM32])
@pytest.mark.parametrize("lookup", _LOOKUPS)
def test_variant_read_before_the_platform_schema_is_rejected(
    lookup: Callable[[], object], platform: str | None
) -> None:
    """Until the platform schema sets the target framework, even esp32 may be Zephyr."""
    CORE.data[KEY_CORE] = {} if platform is None else {KEY_TARGET_PLATFORM: platform}
    with pytest.raises(RuntimeError, match="before the platform schema set it"):
        lookup()


@pytest.mark.parametrize("lookup", _LOOKUPS)
def test_variant_read_on_zephyr_without_zephyr_data_is_rejected(
    lookup: Callable[[], object],
) -> None:
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_STM32,
        KEY_TARGET_FRAMEWORK: "zephyr",
    }
    with pytest.raises(RuntimeError, match="without Zephyr data"):
        lookup()


@pytest.mark.parametrize("platform", ["stm32"])
def test_variant_names_are_unique_and_listed_within_a_platform(platform: str) -> None:
    """VARIANT_MODULES is a dict, so a duplicate name would silently replace a variant."""
    package = import_module(f"esphome.components.{platform}.zephyr_variants")
    names = [
        mod.VARIANT.name
        for info in pkgutil.iter_modules(package.__path__)
        if hasattr(mod := import_module(f"{package.__name__}.{info.name}"), "VARIANT")
    ]
    assert len(names) == len(set(names))
    assert set(names) == set(package.VARIANT_MODULES)
