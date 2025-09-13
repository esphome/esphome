import esphome.config_validation as cv

CODEOWNERS = ["@latonita"]

CONFIG_SCHEMA = cv.invalid(
    "The bmp3xx sensor component has been renamed to bmp3xx_i2c."
)
