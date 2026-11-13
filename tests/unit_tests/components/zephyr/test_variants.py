"""Tests for the Zephyr variants registry."""

from collections.abc import Callable, Iterator, Mapping
import logging
import sys
from types import ModuleType

import pytest

from esphome.components.zephyr import (
    variants,
    zephyr_framework_type,
    zephyr_set_core_data,
    zephyr_variant,
    zephyr_variant_family,
)
from esphome.components.zephyr.const import KEY_BOOTLOADER
from esphome.components.zephyr.framework_helpers import FrameworkSource
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
    PLATFORM_STM32,
)
from esphome.core import CORE

_SDK: ZephyrSDK = ZephyrSDK(
    source=FrameworkSource(
        name="Zephyr",
        url="https://example.com/zephyr",
        optional=("hal_.*",),
        required=frozenset(),
    ),
    workspace_prefix="zephyr",
    default_version="4.5.0",
    min_version=cv.Version(4, 4, 0),
)
_VARIANT: ZephyrVariant = ZephyrVariant(sdk=_SDK, family="chipfam")
_VENDOR_SDK: ZephyrSDK = ZephyrSDK(
    source=FrameworkSource(
        name="Vendor",
        url="https://example.com/vendor",
        optional=(),
        required=frozenset(),
    ),
    workspace_prefix="vendor",
    default_version="2.0.0",
    min_version=cv.Version(2, 0, 0),
)
_ALT_VARIANT: ZephyrVariant = ZephyrVariant(sdk=_SDK, alt_sdks={"vendor": _VENDOR_SDK})
_PACKAGE: str = "esphome.components.fakeplatform.zephyr_variants"


def _module(name: str, **attrs: object) -> ModuleType:
    module = ModuleType(name)
    module.__dict__.update(attrs)
    return module


@pytest.fixture
def fake_platform(monkeypatch: pytest.MonkeyPatch) -> Iterator[ModuleType]:
    """A platform whose zephyr_variants package lists one chip of the chipfam family."""
    chip = _module(f"{_PACKAGE}.chip1", VARIANT_NAME="chip1", VARIANT=_VARIANT)
    family = _module(f"{_PACKAGE}.chipfam_family")
    monkeypatch.setitem(
        sys.modules, _PACKAGE, _module(_PACKAGE, VARIANT_MODULES=["chip1"])
    )
    monkeypatch.setitem(sys.modules, chip.__name__, chip)
    monkeypatch.setitem(sys.modules, family.__name__, family)
    monkeypatch.setattr(variants, "_ZEPHYR_PLATFORMS", ("fakeplatform",))
    yield chip


def _config(
    version: str | None = None, framework_type: str | None = None
) -> dict[str, object]:
    framework: dict[str, str] = {}
    if version is not None:
        framework[CONF_VERSION] = version
    if framework_type is not None:
        framework[CONF_TYPE] = framework_type
    return {CONF_BOARD: "chip1_dk", KEY_BOOTLOADER: "", CONF_FRAMEWORK: framework}


def test_registry_loads_variants_and_families(fake_platform: ModuleType) -> None:
    registry = variants._LazyVariants()

    assert dict(registry.items()) == {"chip1": _VARIANT}
    assert registry.module("chip1") is fake_platform
    assert (
        registry.family_module("chipfam") is sys.modules[f"{_PACKAGE}.chipfam_family"]
    )


@pytest.mark.parametrize(
    "access",
    [
        lambda r: r["chip1"],
        lambda r: r.get("chip1"),
        lambda r: next(iter(r.values())),
        lambda r: _VARIANT if "chip1" in r else None,
        lambda r: _VARIANT if list(r) == ["chip1"] else None,
        lambda r: _VARIANT if list(r.keys()) == ["chip1"] else None,
        lambda r: _VARIANT if r == {"chip1": _VARIANT} else None,
        lambda r: dict(r)["chip1"],
    ],
)
def test_each_lookup_loads_the_registry(
    fake_platform: ModuleType, access: Callable[[Mapping[str, ZephyrVariant]], object]
) -> None:
    assert access(variants._LazyVariants()) is _VARIANT


def test_failed_variant_import_is_raised_on_every_lookup(
    fake_platform: ModuleType,
) -> None:
    sys.modules[_PACKAGE].VARIANT_MODULES = ["chip1", "missing"]
    registry = variants._LazyVariants()

    for _ in range(2):
        with pytest.raises(ModuleNotFoundError):
            registry.get("chip1")
    assert registry._variants == {}


def test_duplicate_variant_name_is_rejected(fake_platform: ModuleType) -> None:
    sys.modules[_PACKAGE].VARIANT_MODULES = ["chip1", "chip1"]
    registry = variants._LazyVariants()

    for _ in range(2):
        with pytest.raises(ValueError, match="are both 'chip1'"):
            registry.get("chip1")


def test_registry_read_while_loading_is_rejected(fake_platform: ModuleType) -> None:
    registry = variants._LazyVariants()
    # VARIANT_NAME is missing, so reading it calls __getattr__, which reads the registry
    del fake_platform.VARIANT_NAME
    fake_platform.__getattr__ = lambda name: registry.get("chip1")

    for _ in range(2):
        with pytest.raises(RuntimeError, match="read VARIANTS while it loaded"):
            registry.get("chip1")


def test_module_lookups(
    fake_platform: ModuleType, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(variants, "_variants_instance", variants._LazyVariants())

    assert variants.get_variant_module("chip1") is fake_platform
    assert variants.get_family_module("chipfam").__name__.endswith("chipfam_family")


def test_registry_rejects_an_unknown_family(fake_platform: ModuleType) -> None:
    with pytest.raises(ValueError, match="Unknown Zephyr family"):
        variants._LazyVariants().family_module("other")


def test_stm32_variants_load() -> None:
    registry = variants._LazyVariants()

    assert list(registry) == ["STM32C5"]
    assert registry.family_module("stm32").__name__.endswith(".stm32_family")


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


def test_version_that_is_not_a_number_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="'v4.5.0' is not a version number") as err:
        resolve_framework_version(_VARIANT, "chip1", _config("v4.5.0"), "chip1 support")
    assert err.value.path == [CONF_FRAMEWORK, CONF_VERSION]


def test_version_below_the_minimum_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match=r"zephyr >= 4.4.0 \(chip1 support\)") as err:
        resolve_framework_version(_VARIANT, "chip1", _config("4.3.0"), "chip1 support")
    assert err.value.path == [CONF_FRAMEWORK, CONF_VERSION]


def test_set_core_data(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(variants, "VARIANTS", {"chip1": _VARIANT})
    CORE.data[KEY_CORE] = {}

    config = {CONF_BOARD: "chip1_dk", CONF_FRAMEWORK: {}}
    set_core_data(
        "chip1",
        config,
        cv.Version(4, 5, 0),
        framework_type="vendor",
        bootloader="mcuboot",
    )

    assert zephyr_variant() == "chip1"
    assert zephyr_variant_family() == "chipfam"
    assert zephyr_framework_type() == "vendor"
    assert CORE.data["zephyr"][CONF_BOARD] == "chip1_dk"
    assert CORE.data["zephyr"][KEY_BOOTLOADER] == "mcuboot"
    assert CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] == "zephyr"
    assert CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] == cv.Version(4, 5, 0)


def test_no_variant_without_the_registry() -> None:
    """nrf52 sets up Zephyr data without a variant; other platforms have none."""
    lookups = (zephyr_variant, zephyr_variant_family, zephyr_framework_type)
    assert [lookup() for lookup in lookups] == [None, None, None]
    zephyr_set_core_data(_config())
    assert [lookup() for lookup in lookups] == [None, None, None]


@pytest.mark.parametrize(
    "lookup", [zephyr_variant, zephyr_variant_family, zephyr_framework_type]
)
def test_variant_read_before_the_platform_schema_is_rejected(
    lookup: Callable[[], str | None],
) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_STM32}
    with pytest.raises(RuntimeError, match="before the platform schema set it"):
        lookup()
