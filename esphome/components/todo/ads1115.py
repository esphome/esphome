from esphome.components import i2c, sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ADDRESS, CONF_ID


DEPENDENCIES = ['i2c']
MULTI_CONF = True

ADS1115Component = sensor.sensor_ns.class_('ADS1115Component', Component, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(ADS1115Component),
    cv.Required(CONF_ADDRESS): cv.i2c_address,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = App.make_ads1115_component(config[CONF_ADDRESS])
    var = Pvariable(config[CONF_ID], rhs)
    register_component(var, config)


BUILD_FLAGS = '-DUSE_ADS1115_SENSOR'
