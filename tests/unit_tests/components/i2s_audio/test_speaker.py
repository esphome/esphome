"""Tests for i2s_audio speaker bit depth validation."""

import pytest

from esphome.components.audio import CONF_MIN_BITS_PER_SAMPLE
from esphome.components.esp32 import (
    KEY_ESP32,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)
from esphome.components.i2s_audio import CONF_I2S_MODE, CONF_PRIMARY
from esphome.components.i2s_audio.speaker import (
    CONF_DAC_TYPE,
    _set_stream_limits,
    _validate_esp32_variant,
)
import esphome.config_validation as cv
from esphome.const import CONF_BITS_PER_SAMPLE
from esphome.core import CORE
from esphome.types import ConfigType


def _config(bits: int) -> ConfigType:
    return {CONF_DAC_TYPE: "external", CONF_BITS_PER_SAMPLE: bits}


@pytest.mark.parametrize("bits", [16, 32])
def test_esp32_accepts_16_and_32_bit(bits: int) -> None:
    """The original ESP32 accepts slots that are a whole number of 16-bit words."""
    CORE.data[KEY_ESP32] = {KEY_VARIANT: VARIANT_ESP32}
    _validate_esp32_variant(_config(bits))


@pytest.mark.parametrize("bits", [8, 24])
def test_esp32_rejects_8_and_24_bit(bits: int) -> None:
    """The original ESP32 rejects slots that do not fill whole 16-bit words."""
    CORE.data[KEY_ESP32] = {KEY_VARIANT: VARIANT_ESP32}
    with pytest.raises(cv.Invalid, match="must be 16bit or 32bit"):
        _validate_esp32_variant(_config(bits))


@pytest.mark.parametrize("bits", [8, 16, 24, 32])
def test_other_variants_accept_all_widths(bits: int) -> None:
    """Newer variants pack samples tightly, so every width is accepted."""
    CORE.data[KEY_ESP32] = {KEY_VARIANT: VARIANT_ESP32S3}
    _validate_esp32_variant(_config(bits))


@pytest.mark.parametrize("variant", [VARIANT_ESP32, VARIANT_ESP32S3])
def test_all_variants_accept_8_bit_streams(variant: str) -> None:
    """Every variant accepts 8-bit streams, since the original ESP32 widens them to its slot."""
    CORE.data[KEY_ESP32] = {KEY_VARIANT: variant}
    config = _set_stream_limits({CONF_I2S_MODE: CONF_PRIMARY})
    assert config[CONF_MIN_BITS_PER_SAMPLE] == 8
