"""The esp32-family Zephyr variants restate ESP-IDF's ADC1 pin->channel table, which
the adc component also carries; the two must not drift apart."""

from __future__ import annotations

import pytest

from esphome.components import esp32
from esphome.components.adc import ESP32_VARIANT_ADC1_PIN_TO_CHANNEL
from esphome.components.zephyr.const import (
    ZEPHYR_VARIANT_ESP32,
    ZEPHYR_VARIANT_ESP32_C3,
    ZEPHYR_VARIANT_ESP32_C5,
    ZEPHYR_VARIANT_ESP32_C6,
    ZEPHYR_VARIANT_ESP32_H2,
)
from esphome.components.zephyr.variants import VARIANTS


@pytest.mark.parametrize(
    ("zephyr_variant", "idf_variant"),
    [
        (ZEPHYR_VARIANT_ESP32, esp32.VARIANT_ESP32),
        (ZEPHYR_VARIANT_ESP32_C3, esp32.VARIANT_ESP32C3),
        (ZEPHYR_VARIANT_ESP32_C5, esp32.VARIANT_ESP32C5),
        (ZEPHYR_VARIANT_ESP32_C6, esp32.VARIANT_ESP32C6),
        (ZEPHYR_VARIANT_ESP32_H2, esp32.VARIANT_ESP32H2),
    ],
)
def test_adc1_channel_map_matches_esp_idf(
    zephyr_variant: str, idf_variant: str
) -> None:
    idf_map = {
        pin: int(str(channel).rsplit("_", 1)[1])
        for pin, channel in ESP32_VARIANT_ADC1_PIN_TO_CHANNEL[idf_variant].items()
    }
    assert VARIANTS[zephyr_variant].adc1_channel_map == idf_map
