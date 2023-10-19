import logging

from esphome.const import (
    CONF_INPUT,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)

import esphome.config_validation as cv
from esphome.pins import check_strapping_pin

_ESP32S2_SPI_PSRAM_PINS = {
    26: "SPICS1",
    27: "SPIHD",
    28: "SPIWP",
    29: "SPICS0",
    30: "SPICLK",
    31: "SPIQ",
    32: "SPID",
}

_ESP32S2_STRAPPING_PINS = {0, 45, 46}

_LOGGER = logging.getLogger(__name__)


def esp32_s2_validate_gpio_pin(value):
    if value < 0 or value > 46:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-46)")

    if value in _ESP32S2_SPI_PSRAM_PINS:
        raise cv.Invalid(
            f"This pin cannot be used on ESP32-S2s and is already used by the SPI/PSRAM interface (function: {_ESP32S2_SPI_PSRAM_PINS[value]})"
        )

    if value in (22, 23, 24, 25):
        # These pins are not exposed in GPIO mux (reason unknown)
        # but they're missing from IO_MUX list in datasheet
        raise cv.Invalid(f"The pin GPIO{value} is not usable on ESP32-S2s.")

    return value


def esp32_s2_validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]
    is_output = mode[CONF_OUTPUT]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]

    if num < 0 or num > 46:
        raise cv.Invalid(f"Invalid pin number: {num} (must be 0-46)")
    if is_input:
        # All ESP32 pins support input mode
        pass
    if is_output and num == 46:
        raise cv.Invalid(
            f"GPIO{num} does not support output pin mode.",
            [CONF_MODE, CONF_OUTPUT],
        )
    if is_pullup and num == 46:
        raise cv.Invalid(
            f"GPIO{num} does not support pullups.", [CONF_MODE, CONF_PULLUP]
        )
    if is_pulldown and num == 46:
        raise cv.Invalid(
            f"GPIO{num} does not support pulldowns.", [CONF_MODE, CONF_PULLDOWN]
        )

    check_strapping_pin(value, _ESP32S2_STRAPPING_PINS, _LOGGER)
    return value
