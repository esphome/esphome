import logging

import esphome.config_validation as cv
from esphome.const import CONF_INPUT, CONF_MODE, CONF_NUMBER
from esphome.pins import check_strapping_pin

_ESP32C5_SPI_PSRAM_PINS = {
    16: "SPICS0",
    17: "SPIQ",
    18: "SPIWP",
    19: "VDD_SPI",
    20: "SPIHD",
    21: "SPICLK",
    22: "SPID",
}

_ESP32C5_STRAPPING_PINS = {2, 7, 27, 28}

_LOGGER = logging.getLogger(__name__)


def esp32_c5_validate_gpio_pin(value):
    if value < 0 or value > 28:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-28)")
    if value in _ESP32C5_SPI_PSRAM_PINS:
        raise cv.Invalid(
            f"This pin cannot be used on ESP32-C5s and is already used by the SPI/PSRAM interface (function: {_ESP32C5_SPI_PSRAM_PINS[value]})"
        )

    return value


def esp32_c5_validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]

    if num < 0 or num > 28:
        raise cv.Invalid(f"Invalid pin number: {num} (must be 0-28)")
    if is_input:
        # All ESP32 pins support input mode
        pass

    check_strapping_pin(value, _ESP32C5_STRAPPING_PINS, _LOGGER)
    return value
