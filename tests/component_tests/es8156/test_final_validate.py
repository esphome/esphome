"""Tests for the es8156 bits_per_sample check on the speakers that use it."""

from __future__ import annotations

import pytest

from esphome.components.es8156.audio_dac import FINAL_VALIDATE_SCHEMA
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUDIO_DAC,
    CONF_BITS_PER_SAMPLE,
    CONF_ID,
    PlatformFramework,
)
from esphome.core import ID
from tests.component_tests.types import SetCoreConfigCallable

ES8156_ID = "es"


def _dac_id(name: str) -> ID:
    return ID(name, is_declaration=False, type="audio_dac::AudioDac")


def _full_config(audio_dac: ID | list[ID], bits_per_sample: int) -> Config:
    """A full config with one speaker using the given audio_dac value."""
    full = Config()
    full["speaker"] = [
        {CONF_AUDIO_DAC: audio_dac, CONF_BITS_PER_SAMPLE: bits_per_sample}
    ]
    return full


def _final_validate() -> None:
    FINAL_VALIDATE_SCHEMA({CONF_ID: ID(ES8156_ID, is_declaration=True)})


def test_single_audio_dac_over_24_bits_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(_dac_id(ES8156_ID), 32)
    )

    with pytest.raises(cv.Invalid, match="ES8156 does not support more than 24 bits"):
        _final_validate()


def test_audio_dac_list_over_24_bits_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The check also finds the es8156 inside a list of audio DACs."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=_full_config([_dac_id("other"), _dac_id(ES8156_ID)], 32),
    )

    with pytest.raises(cv.Invalid, match="ES8156 does not support more than 24 bits"):
        _final_validate()


def test_audio_dac_list_at_16_bits_passes(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=_full_config([_dac_id("other"), _dac_id(ES8156_ID)], 16),
    )

    _final_validate()


def test_other_audio_dac_over_24_bits_passes(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A speaker that does not use this es8156 is not checked."""
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(_dac_id("other"), 32)
    )

    _final_validate()
