import logging

import esphome.config_validation as cv
from esphome.const import CONF_INPUT, CONF_MODE, CONF_NUMBER
from esphome.pins import check_strapping_pin

_ESP32C3_SPI_PSRAM_PINS = {
    12: "SPIHD",
    13: "SPIWP",
    14: "SPICS0",
    15: "SPICLK",
    16: "SPID",
    17: "SPIQ",
}

_ESP32C3_STRAPPING_PINS = {2, 8, 9}

_LOGGER = logging.getLogger(__name__)


def esp32_c3_validate_gpio_pin(value):
    if value < 0 or value > 21:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-21)")
    if value in _ESP32C3_SPI_PSRAM_PINS:
        raise cv.Invalid(
            f"This pin cannot be used on ESP32-C3s and is already used by the SPI/PSRAM interface (function: {_ESP32C3_SPI_PSRAM_PINS[value]})"
        )

    return value


def esp32_c3_validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]

    if num < 0 or num > 21:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-21)")

    if is_input:
        # All ESP32 pins support input mode
        pass

    check_strapping_pin(value, _ESP32C3_STRAPPING_PINS, _LOGGER)
    return value
