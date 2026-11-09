import esphome.config_validation as cv
from esphome.const import CONF_SCL, CONF_SDA

from .gpio_common import ESP32PinSpec, spi_psram_errors

# https://github.com/espressif/esp-idf/blob/master/components/esp_hal_i2c/esp32c5/include/hal/i2c_ll.h
_ESP32C5_I2C_LP_PINS = {"SDA": 2, "SCL": 3}

ESP32_C5_PIN_SPEC = ESP32PinSpec(
    max_pin=28,
    strapping_pins=frozenset({2, 7, 27, 28}),
    usb_jtag_pins=frozenset({13, 14}),
    error_pins=spi_psram_errors(
        "ESP32-C5",
        {
            16: "SPICS0",
            17: "SPIQ",
            18: "SPIWP",
            19: "VDD_SPI",
            20: "SPIHD",
            21: "SPICLK",
            22: "SPID",
        },
    ),
)


def esp32_c5_validate_lp_i2c(value):
    lp_sda_pin = _ESP32C5_I2C_LP_PINS["SDA"]
    lp_scl_pin = _ESP32C5_I2C_LP_PINS["SCL"]
    if int(value[CONF_SDA]) != lp_sda_pin or int(value[CONF_SCL]) != lp_scl_pin:
        raise cv.Invalid(
            f"Low power i2c interface is only supported on GPIO{lp_sda_pin} SDA and GPIO{lp_scl_pin} SCL for ESP32-C5"
        )
    return value
