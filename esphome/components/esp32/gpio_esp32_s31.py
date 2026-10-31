import esphome.config_validation as cv
from esphome.const import CONF_SCL, CONF_SDA

from .gpio_common import ESP32PinSpec

# LP I2C is fixed to GPIO6 (SCL) / GPIO7 (SDA) per the datasheet IO MUX table.
_ESP32S31_I2C_LP_PINS = {"SDA": 7, "SCL": 6}

# Per the ESP32-S31 IDF DOCS and datasheet:
# https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s31/api-reference/peripherals/gpio.html
# https://documentation.espressif.com/esp32-s31_datasheet_en.pdf
ESP32_S31_PIN_SPEC = ESP32PinSpec(
    max_pin=61,
    # GPIO60/GPIO61 set the boot mode; GPIO37 selects the JTAG signal source;
    # GPIO36 sets the VDD_SPI voltage.
    strapping_pins=frozenset({36, 37, 60, 61}),
    usb_jtag_pins=frozenset({33, 34}),
    error_pins={
        num: f"GPIO{num} is reserved for the SPI flash interface on ESP32-S31 and cannot be used."
        for num in (26, 27, 28, 30, 31, 32)
    },
    # Not bonded, so they can never work
    fatal_pins={num: f"GPIO{num} does not exist on ESP32-S31." for num in (29, 41)},
)


def esp32_s31_validate_lp_i2c(value):
    lp_sda_pin = _ESP32S31_I2C_LP_PINS["SDA"]
    lp_scl_pin = _ESP32S31_I2C_LP_PINS["SCL"]
    if int(value[CONF_SDA]) != lp_sda_pin or int(value[CONF_SCL]) != lp_scl_pin:
        raise cv.Invalid(
            f"Low power i2c interface is only supported on GPIO{lp_sda_pin} SDA and GPIO{lp_scl_pin} SCL for ESP32-S31"
        )
    return value
