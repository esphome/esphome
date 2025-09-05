import esphome.config_validation as cv

CODEOWNERS = ["@tchilov"]

CONFIG_SCHEMA = cv.invalid(
    "This component should be used as platform of the Touchscreen component."
)
