import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ADDRESS, CONF_FREQUENCY, CONF_ID, CONF_PHASE_BALANCER
from esphomeyaml.helpers import App, HexIntLiteral, Pvariable, RawExpression, add

DEPENDENCIES = ['i2c']

PHASE_BALANCERS = ['None', 'Linear', 'Weaved']

PCA9685_COMPONENT_TYPE = 'output::PCA9685OutputComponent'

PHASE_BALANCER_MESSAGE = ("The phase_balancer option has been removed in version 1.5.0. "
                          "esphomelib will now automatically choose a suitable phase balancer.")

PCA9685_SCHEMA = vol.Schema({
    cv.GenerateID('pca9685'): cv.register_variable_id,
    vol.Required(CONF_FREQUENCY): vol.All(cv.frequency,
                                          vol.Range(min=23.84, max=1525.88)),
    vol.Optional(CONF_ADDRESS): cv.i2c_address,

    vol.Optional(CONF_PHASE_BALANCER): cv.invalid(PHASE_BALANCER_MESSAGE),
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [PCA9685_SCHEMA])


def to_code(config):
    for conf in config:
        rhs = App.make_pca9685_component(conf.get(CONF_FREQUENCY))
        pca9685 = Pvariable(PCA9685_COMPONENT_TYPE, conf[CONF_ID], rhs)
        if CONF_ADDRESS in conf:
            add(pca9685.set_address(HexIntLiteral(conf[CONF_ADDRESS])))
        if CONF_PHASE_BALANCER in conf:
            phase_balancer = RawExpression(u'PCA9685_PhaseBalancer_{}'.format(
                conf[CONF_PHASE_BALANCER]))
            add(pca9685.set_phase_balancer(phase_balancer))


BUILD_FLAGS = '-DUSE_PCA9685_OUTPUT'
