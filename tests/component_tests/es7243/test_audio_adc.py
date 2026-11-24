"""Tests for the es7243 check that its I2S bus drives MCLK."""

from __future__ import annotations

import pytest

from esphome.components.es7243.audio_adc import FINAL_VALIDATE_SCHEMA
from esphome.components.i2s_audio import CONF_I2S_AUDIO_ID, CONF_I2S_MCLK_PIN
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import CONF_ID, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

BUS_ID = "i2s_audio_bus"
OTHER_BUS_ID = "i2s_audio_other"


def _bus(name: str, *, mclk: bool) -> ConfigType:
    bus: ConfigType = {CONF_ID: ID(name, is_declaration=True, type="I2SAudioComponent")}
    if mclk:
        bus[CONF_I2S_MCLK_PIN] = 0
    return bus


def _full_config(*buses: ConfigType) -> Config:
    """A full config carrying the given i2s_audio buses, with their ids declared as the ID pass does."""
    full = Config()
    full["i2s_audio"] = list(buses)
    for index, bus in enumerate(buses):
        full.declare_ids.append((bus[CONF_ID], ["i2s_audio", index, CONF_ID]))
    return full


def _adc(bus_id: str) -> ConfigType:
    return {
        CONF_I2S_AUDIO_ID: ID(bus_id, is_declaration=False, type="I2SAudioComponent")
    }


def test_bus_with_mclk_passes(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(_bus(BUS_ID, mclk=True))
    )

    FINAL_VALIDATE_SCHEMA(_adc(BUS_ID))


def test_bus_without_mclk_is_rejected(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(_bus(BUS_ID, mclk=False))
    )

    with pytest.raises(
        cv.Invalid, match=f"es7243 needs {CONF_I2S_MCLK_PIN} on i2s_audio {BUS_ID}"
    ):
        FINAL_VALIDATE_SCHEMA(_adc(BUS_ID))


def test_only_the_selected_bus_is_checked(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Another bus without MCLK does not matter, and one with MCLK does not help."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=_full_config(
            _bus(BUS_ID, mclk=True), _bus(OTHER_BUS_ID, mclk=False)
        ),
    )

    FINAL_VALIDATE_SCHEMA(_adc(BUS_ID))
    with pytest.raises(cv.Invalid, match=OTHER_BUS_ID):
        FINAL_VALIDATE_SCHEMA(_adc(OTHER_BUS_ID))
