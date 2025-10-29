import logging

import esphome.config_validation as cv
from esphome.const import CONF_INPUT, CONF_MODE, CONF_NUMBER
from esphome.pins import check_strapping_pin

_ESP32P4_USB_JTAG_PINS = {24, 25}

_ESP32P4_STRAPPING_PINS = {34, 35, 36, 37, 38}

_LOGGER = logging.getLogger(__name__)


def esp32_p4_validate_gpio_pin(value):
    if value < 0 or value > 54:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-54)")
    if value in _ESP32P4_USB_JTAG_PINS:
        _LOGGER.warning(
            "GPIO%d is reserved for the USB-Serial-JTAG interface.\n"
            "To use this pin as GPIO, USB-Serial-JTAG will be disabled.",
            value,
        )

    return value


def esp32_p4_validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]

    if num < 0 or num > 54:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-54)")
    if is_input:
        # All ESP32 pins support input mode
        pass
    check_strapping_pin(value, _ESP32P4_STRAPPING_PINS, _LOGGER)
    return value
