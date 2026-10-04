"""deep_sleep on platform: zephyr is only valid where sys_poweroff() exists."""

from __future__ import annotations

import pytest

from esphome.components.deep_sleep import validate_config
from esphome.components.zephyr.const import KEY_ZEPHYR
from esphome.components.zephyr.variants import VARIANTS
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM, PLATFORM_ZEPHYR
from esphome.core import CORE


def _set_variant(variant: str) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": variant, "family": VARIANTS[variant].family}


@pytest.mark.parametrize(
    "variant", ["EFR32MG24", "SIWX917", "RP2040", "RP2350", "RA4M1", "NATIVESIM"]
)
def test_deep_sleep_rejected_without_poweroff(variant: str) -> None:
    _set_variant(variant)
    with pytest.raises(cv.Invalid, match="deep_sleep is not supported"):
        validate_config({})


@pytest.mark.parametrize("variant", ["ESP32C6", "NRF52", "STM32F4"])
def test_deep_sleep_allowed_with_poweroff(variant: str) -> None:
    _set_variant(variant)
    assert validate_config({}) == {}
