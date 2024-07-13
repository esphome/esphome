import esphome.config_validation as cv

CONFIG_SCHEMA = cv.invalid(
    "The bmp280 sensor component has been renamed to bmp280_i2c."
)
