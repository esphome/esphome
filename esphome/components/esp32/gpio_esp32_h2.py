import logging

from esphome.const import CONF_INPUT, CONF_MODE, CONF_NUMBER

import esphome.config_validation as cv

_ESP32H2_SPI_FLASH_PINS = {6, 7, 15, 16, 17, 18, 19, 20, 21}

_ESP32H2_USB_JTAG_PINS = {26, 27}

_ESP32H2_STRAPPING_PINS = {2, 3, 8, 9, 25}

_LOGGER = logging.getLogger(__name__)


def esp32_h2_validate_gpio_pin(value):
    if value < 0 or value > 27:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-27)")
    if value in _ESP32H2_STRAPPING_PINS:
        _LOGGER.warning(
            "GPIO%d is a Strapping PIN and should be avoided.\n"
            "Attaching external pullup/down resistors to strapping pins can cause unexpected failures.\n"
            "See https://esphome.io/guides/faq.html#why-am-i-getting-a-warning-about-strapping-pins",
            value,
        )
    if value in _ESP32H2_SPI_FLASH_PINS:
        _LOGGER.warning(
            "GPIO%d is reserved for SPI Flash communication on some ESP32-H2 chip variants.\n"
            "Utilizing SPI-reserved pins could cause unexpected failures.\n"
            "See https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/api-reference/peripherals/gpio.html",
            value,
        )
    if value in _ESP32H2_USB_JTAG_PINS:
        _LOGGER.warning(
            "GPIO%d is reserved for the USB-Serial-JTAG interface.\n"
            "To use this pin as GPIO, USB-Serial-JTAG will be disabled.",
            value,
        )

    return value


def esp32_h2_validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]

    if num < 0 or num > 27:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-27)")
    if is_input:
        # All ESP32 pins support input mode
        pass
    return value
