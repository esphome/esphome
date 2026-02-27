import logging

import esphome.config_validation as cv
from esphome.const import CONF_INPUT, CONF_MODE, CONF_NUMBER
from esphome.pins import check_strapping_pin

# GPIO14-17, GPIO19-21 are used for SPI flash/PSRAM
_ESP32C61_SPI_PSRAM_PINS = {
    14: "SPICS0",
    15: "SPICLK",
    16: "SPID",
    17: "SPIQ",
    19: "SPIWP",
    20: "SPIHD",
    21: "VDD_SPI",
}

_ESP32C61_STRAPPING_PINS = {8, 9}

_LOGGER = logging.getLogger(__name__)


def esp32_c61_validate_gpio_pin(value):
    if value < 0 or value > 29:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-29)")
    if value in _ESP32C61_SPI_PSRAM_PINS:
        raise cv.Invalid(
            f"This pin cannot be used on ESP32-C61s and is already used by the SPI/PSRAM interface (function: {_ESP32C61_SPI_PSRAM_PINS[value]})"
        )

    return value


def esp32_c61_validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]

    if num < 0 or num > 29:
        raise cv.Invalid(f"Invalid pin number: {num} (must be 0-29)")
    if is_input:
        # All ESP32-C61 pins support input mode
        pass

    check_strapping_pin(value, _ESP32C61_STRAPPING_PINS, _LOGGER)
    return value
