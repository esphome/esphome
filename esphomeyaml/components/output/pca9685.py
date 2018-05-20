import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import output
from esphomeyaml.components.pca9685 import PCA9685OutputComponent
from esphomeyaml.const import CONF_CHANNEL, CONF_ID, CONF_PCA9685_ID, CONF_POWER_SUPPLY
from esphomeyaml.helpers import Pvariable, get_variable

DEPENDENCIES = ['pca9685']

PLATFORM_SCHEMA = output.PLATFORM_SCHEMA.extend({
    vol.Required(CONF_CHANNEL): vol.All(vol.Coerce(int),
                                        vol.Range(min=0, max=15)),
    vol.Optional(CONF_PCA9685_ID): cv.variable_id,
}).extend(output.FLOAT_OUTPUT_SCHEMA.schema)

Channel = PCA9685OutputComponent.Channel


def to_code(config):
    power_supply = None
    if CONF_POWER_SUPPLY in config:
        power_supply = get_variable(config[CONF_POWER_SUPPLY])
    pca9685 = get_variable(config.get(CONF_PCA9685_ID), PCA9685OutputComponent)
    rhs = pca9685.create_channel(config[CONF_CHANNEL], power_supply)
    out = Pvariable(Channel, config[CONF_ID], rhs)
    output.setup_output_platform(out, config, skip_power_supply=True)


BUILD_FLAGS = '-DUSE_PCA9685_OUTPUT'
