from esphome.const import (
    CONF_NUMBER,
)

import esphome.config_validation as cv


def esp32_s3_validate_gpio_pin(value):
    # Not yet supported
    if value > 0:
        raise cv.Invalid("ESP32-S3 isn't supported yet")
    if value < 0 or value > 48:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-48)")
    return value


def esp32_s3_validate_supports(value):
    num = value[CONF_NUMBER]
    if num < 0 or num > 48:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-48)")
    return value
