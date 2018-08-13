import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components.power_supply import PowerSupplyComponent
from esphomeyaml.const import CONF_INVERTED, CONF_MAX_POWER, CONF_POWER_SUPPLY
from esphomeyaml.helpers import add, esphomelib_ns, get_variable

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

BINARY_OUTPUT_SCHEMA = vol.Schema({
    vol.Optional(CONF_POWER_SUPPLY): cv.use_variable_id(PowerSupplyComponent),
    vol.Optional(CONF_INVERTED): cv.boolean,
})

BINARY_OUTPUT_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(BINARY_OUTPUT_SCHEMA.schema)

FLOAT_OUTPUT_SCHEMA = BINARY_OUTPUT_SCHEMA.extend({
    vol.Optional(CONF_MAX_POWER): cv.percentage,
})

FLOAT_OUTPUT_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(FLOAT_OUTPUT_SCHEMA.schema)

output_ns = esphomelib_ns.namespace('output')
TurnOffAction = output_ns.TurnOffAction
TurnOnAction = output_ns.TurnOnAction
SetLevelAction = output_ns.SetLevelAction


def setup_output_platform_(obj, config, skip_power_supply=False):
    if CONF_INVERTED in config:
        add(obj.set_inverted(config[CONF_INVERTED]))
    if not skip_power_supply and CONF_POWER_SUPPLY in config:
        power_supply = None
        for power_supply in get_variable(config[CONF_POWER_SUPPLY]):
            yield
        add(obj.set_power_supply(power_supply))
    if CONF_MAX_POWER in config:
        add(obj.set_max_power(config[CONF_MAX_POWER]))


def setup_output_platform(obj, config, skip_power_supply=False):
    for _ in setup_output_platform_(obj, config, skip_power_supply):
        yield


BUILD_FLAGS = '-DUSE_OUTPUT'
