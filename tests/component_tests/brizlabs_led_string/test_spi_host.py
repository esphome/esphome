"""Tests for the brizlabs_led_string checks that nothing else uses its SPI host."""

import pytest

from esphome import config_validation as cv
from esphome.components.brizlabs_led_string import DOMAIN
from esphome.components.brizlabs_led_string.light import (
    FINAL_VALIDATE_SCHEMA,
    _final_validate_spi,
)
from esphome.components.spi import CONF_INTERFACE, CONF_INTERFACE_INDEX
from esphome.const import (
    CONF_ETHERNET,
    CONF_LIGHT,
    CONF_PLATFORM,
    CONF_SPI,
    PlatformFramework,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize(
    "full_config",
    [
        {},
        {CONF_SPI: [{}]},  # a software bus has no interface index
        {CONF_SPI: [{CONF_INTERFACE_INDEX: 1}]},
        {CONF_ETHERNET: {}},  # RMII ethernet has no interface
        {CONF_ETHERNET: {CONF_INTERFACE: "spi3"}},
    ],
)
def test_accepts_other_hosts(
    set_core_config: SetCoreConfigCallable, full_config: ConfigType
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    fv.full_config.set(full_config)
    _final_validate_spi({})


@pytest.mark.parametrize(
    ("full_config", "match"),
    [
        ({CONF_SPI: [{}, {CONF_INTERFACE_INDEX: 0}]}, "an `spi` bus are both using"),
        ({CONF_ETHERNET: {CONF_INTERFACE: "spi2"}}, "`ethernet` are both using"),
    ],
)
def test_rejects_spi2(
    set_core_config: SetCoreConfigCallable, full_config: ConfigType, match: str
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    fv.full_config.set(full_config)
    with pytest.raises(cv.Invalid, match=match):
        _final_validate_spi({})


def test_allows_one_instance() -> None:
    brizlabs = {CONF_PLATFORM: DOMAIN}
    # Another light platform does not count
    fv.full_config.set({CONF_LIGHT: [brizlabs, {CONF_PLATFORM: "esp32_rmt_led_strip"}]})
    FINAL_VALIDATE_SCHEMA({})

    fv.full_config.set({CONF_LIGHT: [brizlabs, brizlabs]})
    with pytest.raises(cv.Invalid, match=f"Only one {DOMAIN} is supported"):
        FINAL_VALIDATE_SCHEMA({})
