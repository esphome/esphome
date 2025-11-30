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

# https://github.com/espressif/esp-idf/blob/master/components/soc/esp32c5/include/soc/soc_caps.h
_ESP32C5_I2C_CAPS = {"LP": 1, "HP": 1}

# https://github.com/espressif/esp-idf/blob/master/components/esp_hal_i2c/esp32c5/include/hal/i2c_ll.h
_ESP32C5_I2C_LP_PINS = {"SDA": 2, "SCL": 3}

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


def esp32_c5_using_lp_i2c(value):
    result = False
    sda = int(value[CONF_SDA])
    scl = int(value[CONF_SCL])
    full_config = fv.full_config.get()[CONF_I2C]
    lp_sda_pin = _ESP32C5_I2C_LP_PINS["SDA"]
    lp_scl_pin = _ESP32C5_I2C_LP_PINS["SCL"]
    num_config_i2c = len(full_config)
    max_nbr_i2c = _ESP32C5_I2C_CAPS["HP"] + _ESP32C5_I2C_CAPS["LP"]
    max_nbr_hp_i2c = _ESP32C5_I2C_CAPS["HP"]
    if num_config_i2c > max_nbr_i2c:
        raise cv.Invalid(
            f"The maximum supported i2c interfaces for ESP32-C5 is {max_nbr_i2c}"
        )
    if num_config_i2c > max_nbr_hp_i2c and not any(
        (int(inst[CONF_SDA]) is lp_sda_pin and int(inst[CONF_SCL]) is lp_scl_pin)
        for inst in full_config
    ):
        raise cv.Invalid(
            f"When using {max_nbr_i2c} i2c interfaces for ESP32-C5 you must use low power interface pin {lp_sda_pin} for SDA and {lp_scl_pin} for SCL"
        )
    if (
        num_config_i2c > max_nbr_hp_i2c
        and CONF_LOW_POWER_MODE not in value
        and sda is lp_sda_pin
        and scl is lp_scl_pin
    ):
        result = True
    if (
        num_config_i2c > max_nbr_hp_i2c
        and CONF_LOW_POWER_MODE in value
        and not value[CONF_LOW_POWER_MODE]
        and sda is lp_sda_pin
        and scl is lp_scl_pin
    ):
        raise cv.Invalid(
            f"You must use low power i2c when using more than {max_nbr_hp_i2c} interfaces for ESP32-C5"
        )
    if CONF_LOW_POWER_MODE in value:
        result = value[CONF_LOW_POWER_MODE]
    return result
