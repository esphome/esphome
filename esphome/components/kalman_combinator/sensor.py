import esphome.config_validation as cv

CONFIG_SCHEMA = cv.invalid(
    "The kalman_combinator sensor has moved.\nPlease use the combination platform instead with type: kalman.\n"
    "See https://esphome.io/components/sensor/combination.html"
)
