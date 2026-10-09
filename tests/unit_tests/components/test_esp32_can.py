"""Tests for ESP32 CAN bitrate validation."""

import pytest

from esphome.components.esp32 import KEY_ESP32, KEY_VARIANT
from esphome.components.esp32_can import canbus
import esphome.config_validation as cv
from esphome.core import CORE
import esphome.final_validate as fv


@pytest.mark.parametrize(
    ("revision", "supported"),
    [
        (None, False),
        ("0.0", False),
        ("1.0", False),
        ("1.1", False),
        ("2.0", True),
        ("3.0", True),
        ("3.1", True),
    ],
)
def test_esp32_20kbps_minimum_revision(revision: str | None, supported: bool) -> None:
    """Require the chip revision needed by the ESP-IDF timing macro."""
    CORE.data[KEY_ESP32] = {KEY_VARIANT: "ESP32"}
    advanced = {} if revision is None else {"minimum_chip_revision": revision}
    token = fv.full_config.set({"esp32": {"framework": {"advanced": advanced}}})
    try:
        config = {"bit_rate": canbus.validate_bit_rate("20kbps")}
        if supported:
            canbus.FINAL_VALIDATE_SCHEMA(config)
        else:
            with pytest.raises(cv.Invalid, match="minimum_chip_revision") as error:
                canbus.FINAL_VALIDATE_SCHEMA(config)
            assert error.value.path == ["bit_rate"]
    finally:
        fv.full_config.reset(token)


@pytest.mark.parametrize("variant", ["ESP32", "ESP32C3", "ESP32S3"])
def test_other_bitrates(variant: str) -> None:
    """Existing bitrates must not require a minimum chip revision."""
    CORE.data[KEY_ESP32] = {KEY_VARIANT: variant}
    config = {"bit_rate": canbus.validate_bit_rate("25kbps")}
    canbus.FINAL_VALIDATE_SCHEMA(config)


@pytest.mark.parametrize("variant", ["ESP32C3", "ESP32S2", "ESP32S3", "ESP32C6"])
def test_other_variants_20kbps(variant: str) -> None:
    """Other supported ESP32 variants must retain 20 kbit/s support."""
    CORE.data[KEY_ESP32] = {KEY_VARIANT: variant}
    config = {"bit_rate": canbus.validate_bit_rate("20kbps")}
    canbus.FINAL_VALIDATE_SCHEMA(config)
