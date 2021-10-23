import logging

from esphome.const import (
    CONF_INPUT,
    CONF_MODE,
    CONF_NUMBER,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
import esphome.config_validation as cv

_ESP_32_C3_SPI_PSRAM_PINS = {
    12: "SPIHD",
    13: "SPIWP",
    14: "SPICS0",
    15: "SPICLK",
    16: "SPID",
    17: "SPIQ",
}

_ESP_32_C3_STRAPPING_PINS = {2, 8, 9}

_LOGGER = logging.getLogger(__name__)


def esp32_c3_validate_gpio_pin(value):
    if value < 0 or value > 21:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-21)")
    if value in _ESP_32_C3_SPI_PSRAM_PINS:
        raise cv.Invalid(
            f"This pin cannot be used on ESP32-C3s and is already used by the SPI/PSRAM interface (function: {_ESP_32_C3_SPI_PSRAM_PINS[value]})"
        )
    if value in _ESP_32_C3_STRAPPING_PINS:
        _LOGGER.warning("GPIO%d is a Strapping PIN and should be avoided", value)

    return value


def esp32_c3_validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]

    if num < 0 or num > 21:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-21)")

    if is_input:
        # All ESP32 pins support input mode
        pass
    if is_pullup and num == 21:
        raise cv.Invalid(
            f"GPIO{num} does not support pullups.", [CONF_MODE, CONF_PULLUP]
        )
    if is_pulldown and num == 21:
        raise cv.Invalid(
            f"GPIO{num} does not support pulldowns.", [CONF_MODE, CONF_PULLDOWN]
        )

    return value
