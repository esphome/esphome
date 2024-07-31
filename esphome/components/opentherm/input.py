import esphome.config_validation as cv
from esphome.const import CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP

from . import schema

CONF_AUTO_MIN_VALUE = "auto_min_value"
CONF_AUTO_MAX_VALUE = "auto_max_value"


def validate_min_value_less_than_max_value(config):
    if CONF_MIN_VALUE in config and CONF_MAX_VALUE in config and config[CONF_MIN_VALUE] <= config[CONF_MAX_VALUE]:
        return config

    raise cv.Invalid(f"{CONF_MIN_VALUE} must be less than {CONF_MAX_VALUE}")


def input_schema(entity: schema.InputSchema) -> cv.Schema:
    s = cv.Schema({
        cv.Optional(CONF_MIN_VALUE, entity["range"][0]): cv.float_range(entity["range"][0], entity["range"][1]),
        cv.Optional(CONF_MAX_VALUE, entity["range"][1]): cv.float_range(entity["range"][0], entity["range"][1]),
        cv.Optional(CONF_STEP, entity["step"]): cv.float_
    })

    if CONF_AUTO_MIN_VALUE in entity:
        s = s.extend({cv.Optional(CONF_AUTO_MIN_VALUE, default=False): cv.boolean})

    if CONF_AUTO_MAX_VALUE in entity:
        s = s.extend({cv.Optional(CONF_AUTO_MAX_VALUE, default=False): cv.boolean})

    return s.add_extra(validate_min_value_less_than_max_value)
