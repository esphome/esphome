import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import output
from esphomeyaml.components.pca9685 import PCA9685OutputComponent
from esphomeyaml.const import CONF_CHANNEL, CONF_ID, CONF_PCA9685_ID, CONF_POWER_SUPPLY
from esphomeyaml.helpers import Pvariable, get_variable

DEPENDENCIES = ['pca9685']

Channel = PCA9685OutputComponent.Channel

PLATFORM_SCHEMA = output.FLOAT_OUTPUT_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_ID): cv.declare_variable_id(Channel),
    vol.Required(CONF_CHANNEL): vol.All(vol.Coerce(int),
                                        vol.Range(min=0, max=15)),
    cv.GenerateID(CONF_PCA9685_ID): cv.use_variable_id(PCA9685OutputComponent),
})


def to_code(config):
    power_supply = None
    if CONF_POWER_SUPPLY in config:
        for power_supply in get_variable(config[CONF_POWER_SUPPLY]):
            yield
    pca9685 = None
    for pca9685 in get_variable(config[CONF_PCA9685_ID]):
        yield
    rhs = pca9685.create_channel(config[CONF_CHANNEL], power_supply)
    out = Pvariable(config[CONF_ID], rhs)
    output.setup_output_platform(out, config, skip_power_supply=True)


BUILD_FLAGS = '-DUSE_PCA9685_OUTPUT'
