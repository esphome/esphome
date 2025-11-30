import logging

import esphome.config_validation as cv
from esphome.const import (
    CONF_I2C,
    CONF_INPUT,
    CONF_LOW_POWER_MODE,
    CONF_MODE,
    CONF_NUMBER,
    CONF_SCL,
    CONF_SDA,
)
import esphome.final_validate as fv
from esphome.pins import check_strapping_pin

# https://github.com/espressif/esp-idf/blob/master/components/soc/esp32p4/include/soc/soc_caps.h
_ESP32P4_I2C_CAPS = {"LP": 1, "HP": 2}

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


def esp32_p4_using_lp_i2c(value):
    result = False
    sda = int(value[CONF_SDA])
    scl = int(value[CONF_SCL])
    full_config = fv.full_config.get()[CONF_I2C]
    num_config_i2c = len(full_config)
    max_nbr_i2c = _ESP32P4_I2C_CAPS["HP"] + _ESP32P4_I2C_CAPS["LP"]
    max_nbr_hp_i2c = _ESP32P4_I2C_CAPS["HP"]
    if num_config_i2c > max_nbr_i2c:
        raise cv.Invalid(
            f"The maximum supported i2c interfaces for ESP32-P4 is {max_nbr_i2c}"
        )
    if num_config_i2c > max_nbr_hp_i2c and not any(
        (
            int(inst[CONF_SDA]) in _ESP32P4_LP_PINS
            and int(inst[CONF_SCL]) in _ESP32P4_LP_PINS
        )
        for inst in full_config
    ):
        raise cv.Invalid(
            f"When using {num_config_i2c} i2c interfaces for ESP32-P4 you must use low power interface pins {min(_ESP32P4_LP_PINS)}-{max(_ESP32P4_LP_PINS)}"
        )
    if (
        num_config_i2c > max_nbr_hp_i2c
        and CONF_LOW_POWER_MODE not in value
        and sda in _ESP32P4_LP_PINS
        and scl in _ESP32P4_LP_PINS
    ) and not any(
        (CONF_LOW_POWER_MODE in inst and inst[CONF_LOW_POWER_MODE])
        for inst in full_config
    ):
        result = True
    if CONF_LOW_POWER_MODE in value:
        result = value[CONF_LOW_POWER_MODE]
    return result
