import esphome.config_validation as cv
from esphome.const import CONF_SCL, CONF_SDA

from .gpio_common import ESP32PinSpec, spi_psram_errors

# https://github.com/espressif/esp-idf/blob/master/components/esp_hal_i2c/esp32c6/include/hal/i2c_ll.h
_ESP32C6_I2C_LP_PINS = {"SDA": 6, "SCL": 7}

ESP32_C6_PIN_SPEC = ESP32PinSpec(
    max_pin=30,
    strapping_pins=frozenset({4, 5, 8, 9, 15}),
    usb_jtag_pins=frozenset({12, 13}),
    error_pins=spi_psram_errors(
        "ESP32-C6",
        {
            24: "SPICS0",
            25: "SPIQ",
            26: "SPIWP",
            27: "VDD_SPI",
            28: "SPIHD",
            29: "SPICLK",
            30: "SPID",
        },
    ),
)


def esp32_c6_validate_lp_i2c(value):
    lp_sda_pin = _ESP32C6_I2C_LP_PINS["SDA"]
    lp_scl_pin = _ESP32C6_I2C_LP_PINS["SCL"]
    if int(value[CONF_SDA]) != lp_sda_pin or int(value[CONF_SCL]) != lp_scl_pin:
        raise cv.Invalid(
            f"Low power i2c interface is only supported on GPIO{lp_sda_pin} SDA and GPIO{lp_scl_pin} SCL for ESP32-C6"
        )
    return value
