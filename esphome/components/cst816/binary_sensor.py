import esphome.config_validation as cv

CONFIG_SCHEMA = cv.invalid(
    "The CST816 binary sensor has been removed. Instead use the touchscreen binary sensor with the 'use_raw' flag set."
)
