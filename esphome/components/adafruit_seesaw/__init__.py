# Dummy integration to allow relying on Ardafruit Seesaw API
import esphome.config_validation as cv

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@MrEditor97"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
)
