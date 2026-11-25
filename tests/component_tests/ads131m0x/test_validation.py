"""Tests for ADS131M0x configuration validation."""

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.ads131m0x import SPI_MODES, _validate
from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32, VARIANT_ESP32P4
from esphome.const import (
    CONF_DATA_RATE,
    CONF_GAIN,
    CONF_MODEL,
    CONF_POWER_MODE,
    CONF_SAMPLE_RATE,
    PlatformFramework,
)
from esphome.core import TimePeriod
from tests.component_tests.types import SetCoreConfigCallable


def _config(**overrides: Any) -> dict[str, Any]:
    config = {
        CONF_MODEL: "ADS131M08",
        CONF_GAIN: 1,
        "clock_frequency": 8.192e6,
        CONF_POWER_MODE: "HIGH_RESOLUTION",
        CONF_SAMPLE_RATE: 4000.0,
        "window": TimePeriod(milliseconds=100),
        CONF_DATA_RATE: 8e6,
    }
    config.update(overrides)
    return config


@pytest.fixture
def esp32(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )


@pytest.fixture
def esp32p4(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32P4}
    )


@pytest.mark.usefixtures("esp32")
@pytest.mark.parametrize(
    "overrides",
    [
        {},
        {CONF_GAIN: [1, 2, 4, 8, 16, 32, 64, 128]},
        {CONF_MODEL: "ADS131M02", CONF_SAMPLE_RATE: 32000.0},
        {CONF_SAMPLE_RATE: 250.0},
        {CONF_DATA_RATE: 20e6},
        {
            CONF_POWER_MODE: "LOW_POWER",
            "clock_frequency": 4.096e6,
            CONF_SAMPLE_RATE: 2000.0,
        },
    ],
)
def test_valid(overrides: dict[str, Any]) -> None:
    _validate(_config(**overrides))


@pytest.mark.usefixtures("esp32")
@pytest.mark.parametrize(
    ("overrides", "message"),
    [
        ({CONF_GAIN: [1, 2]}, "gain needs 8 values"),
        ({"clock_frequency": 9e6}, "clock_frequency must be between"),
        (
            {
                CONF_GAIN: 4,
                "clock_frequency": 8.3e6,
                CONF_SAMPLE_RATE: 8.3e6 / 2 / 1024,
            },
            "clock_frequency must be between",
        ),
        ({CONF_POWER_MODE: "VERY_LOW_POWER"}, "clock_frequency must be between"),
        ({CONF_SAMPLE_RATE: 3000.0}, "cannot be reached"),
        ({"window": TimePeriod(seconds=60)}, "the maximum is"),
        ({CONF_DATA_RATE: 40e6}, "at most 25 MHz"),
        (
            {CONF_SAMPLE_RATE: 8000.0, CONF_DATA_RATE: 4e6},
            "too slow for this sample_rate",
        ),
    ],
)
def test_invalid(overrides: dict[str, Any], message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        _validate(_config(**overrides))


@pytest.mark.usefixtures("esp32p4")
def test_p4_frame_is_padded_to_cache_line() -> None:
    # 12-byte frame padded to 64 bytes: 64 us at 8 MHz, more than half of the 31.25 us sample period
    with pytest.raises(cv.Invalid, match="too slow for this sample_rate"):
        _validate(_config(**{CONF_MODEL: "ADS131M02", CONF_SAMPLE_RATE: 32000.0}))
    _validate(_config(**{CONF_SAMPLE_RATE: 4000.0}))
    with pytest.raises(cv.Invalid, match="too slow for this sample_rate"):
        _validate(_config(**{CONF_SAMPLE_RATE: 8000.0}))


@pytest.mark.parametrize("mode", ["MODE1", "mode1", "1", 1])
def test_spi_mode1_accepted(mode: Any) -> None:
    assert cv.enum(SPI_MODES, upper=True)(mode) in SPI_MODES


@pytest.mark.parametrize("mode", ["MODE0", "MODE2", "MODE3", 0, 3])
def test_spi_other_modes_rejected(mode: Any) -> None:
    with pytest.raises(cv.Invalid):
        cv.enum(SPI_MODES, upper=True)(mode)
