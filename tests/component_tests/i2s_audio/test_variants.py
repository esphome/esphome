"""Tests for the per-variant limits of i2s_audio: port count and PDM support."""

import pytest

from esphome import config_validation as cv
from esphome.components import i2s_audio
from esphome.components.esp32 import KEY_VARIANT
from esphome.components.esp32.const import (
    VARIANT_ESP32C3,
    VARIANT_ESP32S2,
    VARIANT_ESP32S31,
)
from esphome.components.i2s_audio import microphone
from esphome.const import CONF_ID, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

CONF_FULL_DUPLEX = i2s_audio.CONF_FULL_DUPLEX


def _set_variant(
    set_core_config: SetCoreConfigCallable, variant: str, buses: int
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: variant},
        full_config={
            i2s_audio.DOMAIN: [
                {CONF_ID: ID(f"bus_{n}"), CONF_FULL_DUPLEX: False} for n in range(buses)
            ]
        },
    )


def test_esp32s31_has_two_i2s_ports(set_core_config: SetCoreConfigCallable) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S31, buses=2)
    i2s_audio._final_validate({})


def test_esp32s31_rejects_a_third_bus(set_core_config: SetCoreConfigCallable) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S31, buses=3)
    with pytest.raises(cv.Invalid, match="Only 2 I2S audio ports"):
        i2s_audio._final_validate({})


def _pdm_microphone(pdm: bool) -> ConfigType:
    return {
        microphone.CONF_ADC_TYPE: "external",
        microphone.CONF_PDM: pdm,
    }


def test_esp32s31_supports_pdm_microphones(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S31, buses=1)
    microphone._validate_esp32_variant(_pdm_microphone(True))


@pytest.mark.parametrize("variant", [VARIANT_ESP32S2, VARIANT_ESP32C3])
def test_pdm_rejected_where_the_soc_has_none(
    set_core_config: SetCoreConfigCallable, variant: str
) -> None:
    _set_variant(set_core_config, variant, buses=1)
    with pytest.raises(cv.Invalid, match="does not support PDM"):
        microphone._validate_esp32_variant(_pdm_microphone(True))
    microphone._validate_esp32_variant(_pdm_microphone(False))
