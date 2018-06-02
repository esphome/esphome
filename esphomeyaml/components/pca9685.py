import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import output
from esphomeyaml.const import CONF_ADDRESS, CONF_FREQUENCY, CONF_ID, CONF_PHASE_BALANCER
from esphomeyaml.helpers import App, HexIntLiteral, Pvariable, add

DEPENDENCIES = ['i2c']

PCA9685OutputComponent = output.output_ns.namespace('PCA9685OutputComponent')

PHASE_BALANCER_MESSAGE = ("The phase_balancer option has been removed in version 1.5.0. "
                          "esphomelib will now automatically choose a suitable phase balancer.")

PCA9685_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(PCA9685OutputComponent),
    vol.Required(CONF_FREQUENCY): vol.All(cv.frequency,
                                          vol.Range(min=23.84, max=1525.88)),
    vol.Optional(CONF_ADDRESS): cv.i2c_address,

    vol.Optional(CONF_PHASE_BALANCER): cv.invalid(PHASE_BALANCER_MESSAGE),
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [PCA9685_SCHEMA])


def to_code(config):
    for conf in config:
        rhs = App.make_pca9685_component(conf.get(CONF_FREQUENCY))
        pca9685 = Pvariable(conf[CONF_ID], rhs)
        if CONF_ADDRESS in conf:
            add(pca9685.set_address(HexIntLiteral(conf[CONF_ADDRESS])))


BUILD_FLAGS = '-DUSE_PCA9685_OUTPUT'
