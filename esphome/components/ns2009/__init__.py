import esphome.config_validation as cv

CODEOWNERS = ["@tchilov"]
DOMAIN = "ns2009"

CONFIG_SCHEMA = cv.invalid(
    "This component should be used as platform of the Touchscreen component."
)
