
import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_POWER_SUPPLY, CONF_INVERTED, CONF_MAX_POWER
from esphomeyaml.helpers import get_variable, add

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_POWER_SUPPLY): cv.variable_id,
    vol.Optional(CONF_INVERTED): cv.boolean,
}).extend(cv.REQUIRED_ID_SCHEMA.schema)

FLOAT_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_MAX_POWER): cv.zero_to_one_float,
})


def setup_output_platform(obj, config, skip_power_supply=False):
    if CONF_INVERTED in config:
        add(obj.set_inverted(config[CONF_INVERTED]))
    if not skip_power_supply and CONF_POWER_SUPPLY in config:
        power_supply = get_variable(config[CONF_POWER_SUPPLY])
        add(obj.set_power_supply(power_supply))
    if CONF_MAX_POWER in config:
        add(obj.set_max_power(config[CONF_MAX_POWER]))


BUILD_FLAGS = '-DUSE_OUTPUT'
