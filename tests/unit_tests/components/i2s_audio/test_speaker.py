"""Tests for I2S audio speaker configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components import esp32
from esphome.components.i2s_audio import (
    CONF_I2S_MODE,
    CONF_MCLK_MULTIPLE,
    CONF_PRIMARY,
    CONF_STEREO,
    CONF_USE_APLL,
)
from esphome.components.i2s_audio.speaker import (
    CONF_DAC_TYPE,
    CONF_EXPAND_TO_SLOT_WIDTH,
    CONF_I2S_COMM_FMT,
    CONF_SPDIF_MODE,
    _final_validate,
    _validate_esp32_variant,
)
from esphome.const import CONF_BITS_PER_SAMPLE, CONF_CHANNEL, CONF_SAMPLE_RATE


def test_spdif_rejects_expand_to_slot_width() -> None:
    config = {
        CONF_DAC_TYPE: "external",
        CONF_I2S_COMM_FMT: "stand_i2s",
        CONF_SPDIF_MODE: True,
        CONF_EXPAND_TO_SLOT_WIDTH: True,
        CONF_SAMPLE_RATE: 48000,
        CONF_CHANNEL: CONF_STEREO,
        CONF_USE_APLL: True,
        CONF_I2S_MODE: CONF_PRIMARY,
        CONF_MCLK_MULTIPLE: 256,
    }

    with pytest.raises(cv.Invalid, match="not supported in SPDIF mode"):
        _final_validate(config)


def test_original_esp32_rejects_expansion_to_24_bit(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(esp32, "get_esp32_variant", lambda: esp32.VARIANT_ESP32)
    config = {
        CONF_DAC_TYPE: "external",
        CONF_BITS_PER_SAMPLE: 24,
        CONF_EXPAND_TO_SLOT_WIDTH: True,
    }

    with pytest.raises(cv.Invalid, match="only to 16- or 32-bit slots"):
        _validate_esp32_variant(config)


def test_original_esp32_accepts_expansion_to_32_bit(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(esp32, "get_esp32_variant", lambda: esp32.VARIANT_ESP32)
    config = {
        CONF_DAC_TYPE: "external",
        CONF_BITS_PER_SAMPLE: 32,
        CONF_EXPAND_TO_SLOT_WIDTH: True,
    }

    assert _validate_esp32_variant(config) == config
