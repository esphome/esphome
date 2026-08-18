"""Tests for the i2s_audio microphone and speaker schemas."""

import pytest

from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32S3
import esphome.config_validation as cv
from esphome.const import CONF_ESPHOME, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


@pytest.fixture
def set_esp32_s3(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
        full_config={CONF_ESPHOME: {}},
    )


def test_microphone_rejects_internal_adc_type(set_esp32_s3: None) -> None:
    """adc_type: internal was removed with the legacy I2S driver and is no longer in the schema."""
    from esphome.components.i2s_audio.microphone import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match=r"Unknown value 'internal'"):
        CONFIG_SCHEMA({"adc_type": "internal", "adc_pin": "GPIO32"})


def test_speaker_rejects_internal_dac_type(set_esp32_s3: None) -> None:
    """dac_type: internal was removed with the legacy I2S driver and is no longer in the schema."""
    from esphome.components.i2s_audio.speaker import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match=r"Unknown value 'internal'"):
        CONFIG_SCHEMA({"dac_type": "internal", "mode": "mono"})


def test_speaker_rejects_stand_max_comm_fmt(set_esp32_s3: None) -> None:
    """i2s_comm_fmt: stand_max was removed with the legacy I2S driver and is no longer in the schema."""
    from esphome.components.i2s_audio.speaker import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match=r"Unknown value 'stand_max'"):
        CONFIG_SCHEMA(
            {"dac_type": "external", "i2s_dout_pin": 5, "i2s_comm_fmt": "stand_max"}
        )


def test_external_schemas_still_validate(set_esp32_s3: None) -> None:
    """The surviving external variants still pass schema validation."""
    from esphome.components.i2s_audio.microphone import (
        CONFIG_SCHEMA as MIC_CONFIG_SCHEMA,
    )
    from esphome.components.i2s_audio.speaker import (
        CONFIG_SCHEMA as SPEAKER_CONFIG_SCHEMA,
    )

    MIC_CONFIG_SCHEMA({"adc_type": "external", "i2s_din_pin": 4, "channel": "left"})
    SPEAKER_CONFIG_SCHEMA({"dac_type": "external", "i2s_dout_pin": 5})
