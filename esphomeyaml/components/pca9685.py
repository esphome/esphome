import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ADDRESS, CONF_FREQUENCY, CONF_ID, CONF_PHASE_BALANCER
from esphomeyaml.helpers import App, HexIntLiteral, Pvariable, RawExpression, add

DEPENDENCIES = ['i2c']

PHASE_BALANCERS = ['None', 'Linear', 'Weaved']

PCA9685_COMPONENT_TYPE = 'output::PCA9685OutputComponent'

PCA9685_SCHEMA = vol.Schema({
    cv.GenerateID('pca9685'): cv.register_variable_id,
    vol.Required(CONF_FREQUENCY): vol.All(cv.frequency,
                                          vol.Range(min=24, max=1526)),
    vol.Optional(CONF_PHASE_BALANCER): vol.All(vol.Title, vol.Any(*PHASE_BALANCERS)),
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
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
