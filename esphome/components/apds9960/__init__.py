import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL

DEPENDENCIES = ['i2c']
AUTO_LOAD = ['sensor', 'binary_sensor']
MULTI_CONF = True

CONF_APDS9960_ID = 'apds9960_id'

apds9960_nds = cg.esphome_ns.namespace('apds9960')
APDS9960 = apds9960_nds.class_('APDS9960', cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(APDS9960),
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x39))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_UPDATE_INTERVAL])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
