import logging
from typing import Any

import esphome.config_validation as cv
from esphome.const import CONF_INPUT, CONF_MODE, CONF_NUMBER, CONF_SCL, CONF_SDA
from esphome.pins import check_strapping_pin

# Per the ESP32-S31 datasheet (page 96):
# https://documentation.espressif.com/esp32-s31_datasheet_en.pdf
_ESP32S31_SPI_FLASH_PINS: set[int] = {27, 28, 29, 31, 32, 33}
# GPIO60/GPIO61 set the boot mode; GPIO37 selects the JTAG signal source.
_ESP32S31_STRAPPING_PINS: set[int] = {37, 60, 61}
# LP I2C is fixed to GPIO6 (SCL) / GPIO7 (SDA) per the datasheet IO MUX table.
_ESP32S31_I2C_LP_PINS = {"SDA": 7, "SCL": 6}

_LOGGER = logging.getLogger(__name__)


def esp32_s31_validate_gpio_pin(value: int) -> int:
    if value < 0 or value > 61:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-61)")
    if value in _ESP32S31_SPI_FLASH_PINS:
        raise cv.Invalid(
            f"GPIO{value} is reserved for the SPI flash interface on ESP32-S31 and cannot be used."
        )
    return value


def esp32_s31_validate_supports(value: dict[str, Any]) -> dict[str, Any]:
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]

    if num < 0 or num > 61:
        raise cv.Invalid(f"Invalid pin number: {num} (must be 0-61)")
    if is_input:
        # All ESP32 pins support input mode
        pass

    check_strapping_pin(value, _ESP32S31_STRAPPING_PINS, _LOGGER)
    return value


def esp32_s31_validate_lp_i2c(value):
    lp_sda_pin = _ESP32S31_I2C_LP_PINS["SDA"]
    lp_scl_pin = _ESP32S31_I2C_LP_PINS["SCL"]
    if int(value[CONF_SDA]) != lp_sda_pin or int(value[CONF_SCL]) != lp_scl_pin:
        raise cv.Invalid(
            f"Low power i2c interface is only supported on GPIO{lp_sda_pin} SDA and GPIO{lp_scl_pin} SCL for ESP32-S31"
        )
    return value
