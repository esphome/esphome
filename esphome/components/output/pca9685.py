import voluptuous as vol

from esphome.components import output
from esphome.components.pca9685 import PCA9685OutputComponent
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_PCA9685_ID, CONF_POWER_SUPPLY
from esphome.cpp_generator import Pvariable, get_variable

DEPENDENCIES = ['pca9685']

Channel = PCA9685OutputComponent.class_('Channel', output.FloatOutput)

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
    for pca9685 in get_variable(config[CONF_PCA9685_ID]):
        yield
    rhs = pca9685.create_channel(config[CONF_CHANNEL], power_supply)
    out = Pvariable(config[CONF_ID], rhs)
    output.setup_output_platform(out, config, skip_power_supply=True)


BUILD_FLAGS = '-DUSE_PCA9685_OUTPUT'
