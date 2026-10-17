"""Tests for ES7210 audio ADC configuration validation."""

import pytest

from esphome.components.es7210.audio_adc import (
    CONF_CHANNEL_GAINS,
    CONF_TDM,
    CONFIG_SCHEMA,
)
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import CONF_MIC_GAIN, KEY_VARIANT, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_channel_gains_valid_config() -> None:
    """Per-channel gains accept an AEC microphone and reference configuration."""
    config = CONFIG_SCHEMA({CONF_CHANNEL_GAINS: ["33db", "33db", "0db", "0db"]})

    assert config[CONF_CHANNEL_GAINS] == [33.0, 33.0, 0.0, 0.0]
    assert CONF_MIC_GAIN not in config


def test_omitted_gains_remain_absent() -> None:
    """Code generation supplies the default when neither gain option is configured."""
    config = CONFIG_SCHEMA({})

    assert CONF_MIC_GAIN not in config
    assert CONF_CHANNEL_GAINS not in config


@pytest.mark.parametrize("variant", [VARIANT_ESP32, VARIANT_ESP32S2])
def test_tdm_rejected_on_unsupported_variants(
    variant: str, set_core_config: SetCoreConfigCallable
) -> None:
    """ESP32 and ESP32-S2 cannot receive TDM audio."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: variant})

    with pytest.raises(cv.Invalid, match="ES7210 TDM mode is not available") as exc:
        CONFIG_SCHEMA({CONF_TDM: True})

    assert exc.value.path == [CONF_TDM]


@pytest.mark.parametrize("variant", [VARIANT_ESP32, VARIANT_ESP32S2])
@pytest.mark.parametrize("tdm_config", [{}, {CONF_TDM: False}])
def test_tdm_disabled_on_unsupported_variants(
    variant: str,
    tdm_config: dict[str, bool],
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Unsupported variants still accept standard I2S mode."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: variant})

    assert CONFIG_SCHEMA(tdm_config)[CONF_TDM] is False


@pytest.mark.parametrize("variant", [VARIANT_ESP32S3, VARIANT_ESP32C3, VARIANT_ESP32P4])
def test_tdm_accepted_on_supported_variants(
    variant: str, set_core_config: SetCoreConfigCallable
) -> None:
    """Variants with TDM hardware accept TDM mode."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: variant})

    assert CONFIG_SCHEMA({CONF_TDM: True})[CONF_TDM] is True


@pytest.mark.parametrize("channel_count", [3, 5])
def test_channel_gains_requires_four_channels(channel_count: int) -> None:
    """Channel gains must provide one value for each ES7210 input."""
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({CONF_CHANNEL_GAINS: ["0db"] * channel_count})


def test_channel_gains_and_mic_gain_are_mutually_exclusive() -> None:
    """Uniform and per-channel gain configuration cannot be combined."""
    with pytest.raises(cv.Invalid, match="Cannot specify more than one"):
        CONFIG_SCHEMA(
            {
                CONF_MIC_GAIN: "24db",
                CONF_CHANNEL_GAINS: ["0db"] * 4,
            }
        )
