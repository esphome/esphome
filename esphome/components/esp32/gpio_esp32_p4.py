import logging

import esphome.config_validation as cv
from esphome.const import CONF_INPUT, CONF_MODE, CONF_NUMBER, CONF_SCL, CONF_SDA
from esphome.pins import check_strapping_pin

# https://documentation.espressif.com/esp32-p4-chip-revision-v1.3_datasheet_en.pdf
_ESP32P4_LP_PINS = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}

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


def esp32_p4_validate_lp_i2c(value):
    if (
        int(value[CONF_SDA]) not in _ESP32P4_LP_PINS
        or int(value[CONF_SCL]) not in _ESP32P4_LP_PINS
    ):
        raise cv.Invalid(
            f"Low power i2c interface for ESP32-P4 is only supported on low power interface GPIO{min(_ESP32P4_LP_PINS)} - GPIO{max(_ESP32P4_LP_PINS)}"
        )
    return value
