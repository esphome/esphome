"""Tests for usb_cdc_acm config validation."""

import pytest

from esphome import config_validation as cv
from esphome.components import usb_cdc_acm
from esphome.components.nrf52.const import (
    VARIANT_NRF54L15,
    VARIANT_NRF51822,
    VARIANT_NRF52832,
    VARIANT_NRF52840,
)
from esphome.components.nrf52.framework import set_nrf52_variant
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize(
    "variant", [VARIANT_NRF51822, VARIANT_NRF52832, VARIANT_NRF54L15]
)
def test_nrf52_variant_without_usb_rejected(
    set_core_config: SetCoreConfigCallable, variant: str
) -> None:
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    set_nrf52_variant(variant)
    with pytest.raises(cv.Invalid, match=f"needs USB, which the {variant}"):
        usb_cdc_acm.CONFIG_SCHEMA({})


def test_nrf52_variant_with_usb_accepted(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    set_nrf52_variant(VARIANT_NRF52840)
    usb_cdc_acm.CONFIG_SCHEMA({})
