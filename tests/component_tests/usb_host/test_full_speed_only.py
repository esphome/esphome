"""Tests for the usb_host full_speed_only option."""

from collections.abc import Callable

import pytest

from esphome import config_validation as cv
from esphome.components import usb_host
from esphome.components.esp32 import (
    VARIANT_ESP32H4,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
)
from esphome.const import KEY_IDF_VERSION, KEY_VARIANT, PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable

DEFINE = "USE_USB_HOST_FULL_SPEED_ONLY"


def _set_variant(set_core_config: SetCoreConfigCallable, variant: str) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: variant, KEY_IDF_VERSION: cv.Version(6, 1, 1)},
    )


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def test_full_speed_only_emits_the_define(
    generate_main: Callable[[str], str],
) -> None:
    generate_main("tests/component_tests/usb_host/usb_host_full_speed_only.yaml")
    assert DEFINE in _defines()


def test_full_speed_only_is_off_by_default(
    generate_main: Callable[[str], str],
) -> None:
    generate_main("tests/component_tests/usb_host/usb_host.yaml")
    assert DEFINE not in _defines()


@pytest.mark.parametrize("value", [False, True])
def test_full_speed_only_is_accepted_on_the_p4(
    set_core_config: SetCoreConfigCallable, value: bool
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32P4)
    config = usb_host.CONFIG_SCHEMA({usb_host.CONF_FULL_SPEED_ONLY: value})
    assert config[usb_host.CONF_FULL_SPEED_ONLY] is value


def test_full_speed_only_defaults_to_false(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32P4)
    assert usb_host.CONFIG_SCHEMA({})[usb_host.CONF_FULL_SPEED_ONLY] is False


@pytest.mark.parametrize(
    "variant", [VARIANT_ESP32S2, VARIANT_ESP32S3, VARIANT_ESP32S31, VARIANT_ESP32H4]
)
def test_full_speed_only_is_refused_without_a_high_speed_port(
    set_core_config: SetCoreConfigCallable, variant: str
) -> None:
    _set_variant(set_core_config, variant)
    with pytest.raises(cv.Invalid, match="only for the high-speed port"):
        usb_host.CONFIG_SCHEMA({usb_host.CONF_FULL_SPEED_ONLY: True})
    # Leaving it off is fine on every variant
    assert usb_host.CONFIG_SCHEMA({})[usb_host.CONF_FULL_SPEED_ONLY] is False
