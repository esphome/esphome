"""Tests for the shared Zephyr variant code."""

from collections.abc import Callable
from importlib import import_module
import logging
import pkgutil
from types import ModuleType

import pytest

from esphome.components.zephyr import (
    zephyr_framework_type,
    zephyr_set_core_data,
    zephyr_variant,
    zephyr_variant_family,
)
from esphome.components.zephyr.const import KEY_BOOTLOADER
from esphome.components.zephyr.variants import (
    ZEPHYR_MAINLINE,
    ZephyrSDK,
    ZephyrVariant,
    resolve_framework_version,
    set_core_data,
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

_SDK: ZephyrSDK = ZephyrSDK(
    default_version="4.5.0",
    min_version=cv.Version(4, 4, 0),
)
_FAMILY: ModuleType = ModuleType("chipfam_family")
_FAMILY.FAMILY = "chipfam"
_VARIANT: ZephyrVariant = ZephyrVariant(sdk=_SDK, family=_FAMILY)
_VENDOR_SDK: ZephyrSDK = ZephyrSDK(
    default_version="2.0.0",
    min_version=cv.Version(2, 0, 0),
)
_ALT_VARIANT: ZephyrVariant = ZephyrVariant(sdk=_SDK, alt_sdks={"vendor": _VENDOR_SDK})


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
def test_version_defaults_to_the_sdks(
    version: str | None, framework_type: str | None, caplog: pytest.LogCaptureFixture
) -> None:
    assert resolve_framework_version(
        _VARIANT, "chip1", _config(version, framework_type), "chip1 support"
    ) == ("4.5.0", cv.Version(4, 5, 0), "zephyr", _SDK)
    assert "not the recommended one" not in caplog.text


def test_type_selects_an_alternative_sdk() -> None:
    assert resolve_framework_version(
        _ALT_VARIANT, "chip1", _config(framework_type="vendor"), "chip1 support"
    ) == ("2.0.0", cv.Version(2, 0, 0), "vendor", _VENDOR_SDK)


def test_unknown_type_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match=r"Valid: \['zephyr', 'vendor'\]") as err:
        resolve_framework_version(
            _ALT_VARIANT, "chip1", _config(framework_type="other"), "chip1 support"
        )
    assert err.value.path == [CONF_FRAMEWORK, CONF_TYPE]


def test_version_other_than_the_default_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level(logging.WARNING):
        _, framework_ver, _, _ = resolve_framework_version(
            _VARIANT, "chip1", _config("4.4.0"), "chip1 support"
        )
    assert framework_ver == cv.Version(4, 4, 0)
    assert "not the recommended one (4.5.0)" in caplog.text


@pytest.mark.parametrize("version", ["v4.5.0", "4.5.0-rc1a"])
def test_version_that_is_not_a_number_is_rejected(version: str) -> None:
    with pytest.raises(cv.Invalid, match=f"'{version}' is not a version number") as err:
        resolve_framework_version(_VARIANT, "chip1", _config(version), "chip1 support")
    assert err.value.path == [CONF_FRAMEWORK, CONF_VERSION]


def test_version_below_the_minimum_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match=r"zephyr >= 4.4.0 \(chip1 support\)") as err:
        resolve_framework_version(_VARIANT, "chip1", _config("4.3.0"), "chip1 support")
    assert err.value.path == [CONF_FRAMEWORK, CONF_VERSION]


def test_prerelease_of_the_minimum_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match=r"zephyr >= 4.4.0"):
        resolve_framework_version(
            _VARIANT, "chip1", _config("4.4.0-rc1"), "chip1 support"
        )


def test_release_meets_a_prerelease_minimum() -> None:
    sdk = ZephyrSDK(default_version="4.5.0", min_version=cv.Version(4, 5, 0, "rc2"))
    variant = ZephyrVariant(sdk=sdk)
    for version in ("4.5.0", "4.5.0-rc10"):
        resolve_framework_version(variant, "chip1", _config(version), "chip1 support")


@pytest.mark.parametrize(
    ("variant", "family"), [(_VARIANT, "chipfam"), (_ALT_VARIANT, None)]
)
def test_set_core_data(variant: ZephyrVariant, family: str | None) -> None:
    CORE.data[KEY_CORE] = {}

    config = {CONF_BOARD: "chip1_dk", CONF_FRAMEWORK: {}}
    set_core_data(
        "chip1",
        variant,
        config,
        cv.Version(4, 5, 0),
        framework_type="vendor",
        bootloader="mcuboot",
    )

    assert zephyr_variant() == "chip1"
    assert zephyr_variant_family() == family
    assert zephyr_framework_type() == "vendor"
    assert CORE.data["zephyr"][CONF_BOARD] == "chip1_dk"
    assert CORE.data["zephyr"][KEY_BOOTLOADER] == "mcuboot"
    assert CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] == "zephyr"
    assert CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] == cv.Version(4, 5, 0)


def test_no_variant_on_nrf52_or_other_platforms() -> None:
    """nrf52 sets up Zephyr data without a variant; other platforms have none."""
    lookups = (zephyr_variant, zephyr_variant_family, zephyr_framework_type)
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_ESP32,
        KEY_TARGET_FRAMEWORK: "esp-idf",
    }
    assert [lookup() for lookup in lookups] == [None, None, None]
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_NRF52,
        KEY_TARGET_FRAMEWORK: "zephyr",
    }
    zephyr_set_core_data(_config())
    assert [lookup() for lookup in lookups] == [None, None, None]


@pytest.mark.parametrize("platform", [None, PLATFORM_ESP32, PLATFORM_STM32])
@pytest.mark.parametrize(
    "lookup", [zephyr_variant, zephyr_variant_family, zephyr_framework_type]
)
def test_variant_read_before_the_platform_schema_is_rejected(
    lookup: Callable[[], str | None], platform: str | None
) -> None:
    """Until the platform schema sets the target framework, even esp32 may be Zephyr."""
    CORE.data[KEY_CORE] = {} if platform is None else {KEY_TARGET_PLATFORM: platform}
    with pytest.raises(RuntimeError, match="before the platform schema set it"):
        lookup()


@pytest.mark.parametrize("platform", ["stm32"])
def test_variant_names_are_unique_and_listed_within_a_platform(platform: str) -> None:
    """VARIANT_MODULES is a dict, so a duplicate name would silently replace a variant."""
    package = import_module(f"esphome.components.{platform}.zephyr_variants")
    names = [
        mod.VARIANT_NAME
        for info in pkgutil.iter_modules(package.__path__)
        if hasattr(
            mod := import_module(f"{package.__name__}.{info.name}"), "VARIANT_NAME"
        )
    ]
    assert len(names) == len(set(names))
    assert set(names) == set(package.VARIANT_MODULES)
