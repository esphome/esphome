import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ADDRESS, CONF_ID, CONF_RATE
from esphomeyaml.helpers import App, Pvariable, RawExpression, add, HexIntLiteral

DEPENDENCIES = ['i2c']

ADS1115_COMPONENT_CLASS = 'sensor::ADS1115Component'

RATES = {
    8: 'ADS1115_RATE_8',
    16: 'ADS1115_RATE_16',
    32: 'ADS1115_RATE_32',
    64: 'ADS1115_RATE_64',
    128: 'ADS1115_RATE_128',
    250: 'ADS1115_RATE_250',
    475: 'ADS1115_RATE_475',
    860: 'ADS1115_RATE_860',
}

ADS1115_SCHEMA = vol.Schema({
    cv.GenerateID('ads1115'): cv.register_variable_id,
    vol.Required(CONF_ADDRESS): cv.i2c_address,
    vol.Optional(CONF_RATE): vol.All(vol.Coerce(int), vol.Any(*list(RATES.keys()))),
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [ADS1115_SCHEMA])


def to_code(config):
    for conf in config:
        address = HexIntLiteral(conf[CONF_ADDRESS])
        rhs = App.make_ads1115_component(address)
        ads1115 = Pvariable(ADS1115_COMPONENT_CLASS, conf[CONF_ID], rhs)
        if CONF_RATE in conf:
            add(ads1115.set_rate(RawExpression(RATES[conf[CONF_RATE]])))


BUILD_FLAGS = '-DUSE_ADS1115_SENSOR'
