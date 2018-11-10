import voluptuous as vol

from esphomeyaml.components import sensor, i2c
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ADDRESS, CONF_ID
from esphomeyaml.helpers import App, Pvariable, setup_component, Component

DEPENDENCIES = ['i2c']

ADS1115Component = sensor.sensor_ns.class_('ADS1115Component', Component, i2c.I2CDevice)

ADS1115_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(ADS1115Component),
    vol.Required(CONF_ADDRESS): cv.i2c_address,
}).extend(cv.COMPONENT_SCHEMA.schema)

CONFIG_SCHEMA = vol.All(cv.ensure_list, [ADS1115_SCHEMA])


def to_code(config):
    for conf in config:
        rhs = App.make_ads1115_component(conf[CONF_ADDRESS])
        var = Pvariable(conf[CONF_ID], rhs)
        setup_component(var, conf)


BUILD_FLAGS = '-DUSE_ADS1115_SENSOR'
